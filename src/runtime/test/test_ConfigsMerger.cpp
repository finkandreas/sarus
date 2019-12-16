/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "common/ImageMetadata.hpp"
#include "runtime/ConfigsMerger.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace runtime {
namespace test {

TEST_GROUP(ConfigsMergerTestGroup) {
};

std::vector<std::unordered_map<std::string, std::string>> getHooksEnvironments(const rapidjson::Value& hooks) {
    auto envs = std::vector<std::unordered_map<std::string, std::string>>{};

    for(const auto& hookType : {"prestart", "poststart", "poststop"}) {
        if(!hooks.HasMember(hookType)) {
            continue;
        }

        for(auto& hook : hooks[hookType].GetArray()) {
            if(!hook.HasMember("env")) {
                continue;
            }
            auto env = std::unordered_map<std::string, std::string>{};
            for(const auto& var : hook["env"].GetArray()) {
                std::string key;
                std::string value;
                std::tie(key, value) = common::parseEnvironmentVariable(var.GetString());
                env[key] = value;
            }
            envs.push_back(std::move(env));
        }
    }

    CHECK(!envs.empty());

    return envs;
}

TEST(ConfigsMergerTestGroup, hooks) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    // check that the hooks's environment variables are not set yet
    CHECK(config->json.HasMember("OCIHooks"));
    auto envs = getHooksEnvironments(configRAII.config->json["OCIHooks"]);
    for(const auto& env : envs) {
        CHECK(env.find("key0") == env.cend());
        CHECK(env.find("key1") == env.cend());
    }

    config->commandRun.hooksEnvironment["key0"] = "value0";
    config->commandRun.hooksEnvironment["key1"] = "value1";

    // check that ConfigsMerger generates hooks's JSON
    // with the expected environment variables
    auto metadata = common::ImageMetadata{};
    auto configsMerger = ConfigsMerger{config, metadata};
    auto doc = rapidjson::Document{};
    auto hooks = configsMerger.getHooks(doc.GetAllocator());
    envs = getHooksEnvironments(hooks);
    for(const auto& env : envs) {
        CHECK(env.find("key0") != env.cend());
        CHECK(env.find("key1") != env.cend());
    }
}

TEST(ConfigsMergerTestGroup, cwd) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto metadata = common::ImageMetadata{};

    // no cwd in metadata
    CHECK((ConfigsMerger{config, metadata}.getCwdInContainer() == "/"));

    // cwd in metadata
    metadata.workdir = "/workdir-from-metadata";
    CHECK((ConfigsMerger{config, metadata}.getCwdInContainer() == "/workdir-from-metadata"));
}

TEST(ConfigsMergerTestGroup, environment) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto metadata = common::ImageMetadata{};

    // only host environment
    {
        config->commandRun.hostEnvironment = {{"KEY", "HOST_VALUE"}};
        metadata.env = {};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"KEY", "HOST_VALUE"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // only metadata environment
    {
        config->commandRun.hostEnvironment = {};
        metadata.env = {{"KEY", "CONTAINER_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"KEY", "CONTAINER_VALUE"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // metadata environment overrides host environment
    {
        config->commandRun.hostEnvironment = {{"KEY", "HOST_VALUE"}};
        metadata.env = {{"KEY", "CONTAINER_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"KEY", "CONTAINER_VALUE"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
}

void checkNvidiaEnvironmentVariables(const std::unordered_map<std::string, std::string>& resultEnvironment,
                                 const std::string expectedNvidiaVisibleDevices,
                                 const std::string expectedCudaVisibleDevices,
                                 const std::string expectedDriverCapabilities = "all") {
    if (expectedNvidiaVisibleDevices.empty()) {
        CHECK(resultEnvironment.find("CUDA_VISIBLE_DEVICES") == resultEnvironment.end());
        CHECK(resultEnvironment.find("NVIDIA_VISIBLE_DEVICES") == resultEnvironment.end());
        CHECK(resultEnvironment.find("NVIDIA_DRIVER_CAPABILITIES") == resultEnvironment.end());
    }
    else {
        CHECK(resultEnvironment.at("CUDA_VISIBLE_DEVICES") == expectedCudaVisibleDevices);
        CHECK(resultEnvironment.at("NVIDIA_VISIBLE_DEVICES") == expectedNvidiaVisibleDevices);
        CHECK(resultEnvironment.at("NVIDIA_DRIVER_CAPABILITIES") == expectedDriverCapabilities);
    }
}

TEST(ConfigsMergerTestGroup, nvidia_environment) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto metadata = common::ImageMetadata{};

    // Single device
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "0"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "0", "0");
    }
    // Single device, not 1st one, selected driver capabilities
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "1"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"NVIDIA_DRIVER_CAPABILITIES", "utility,compute"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "1", "0", "utility,compute");
    }
    // CUDA_VISIBLE_DEVICES in image
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "1"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"CUDA_VISIBLE_DEVICES", "0,1"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "1", "0");
    }
    // No host CUDA_VISIBLE_DEVICES
    {
        config->commandRun.hostEnvironment = {};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"NVIDIA_DRIVER_CAPABILITIES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "", "");
    }
    // Host CUDA_VISIBLE_DEVICES set to NoDevFiles
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "NoDevFiles"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"NVIDIA_DRIVER_CAPABILITIES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "", "");
    }
    // Multiple devices in order
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "1,2"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "1,2", "0,1");
    }
    // Shuffled selection
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "3,1,5"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "3,1,5", "1,0,2");
    }
}

TEST(ConfigsMergerTestGroup, hooks_environment) {
    auto metadata = common::ImageMetadata{};

    // No hooks enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.hostEnvironment = {};
        metadata.env = {};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // MPI hook enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.useMPI = true;
        config->commandRun.hostEnvironment = {};
        metadata.env = {};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"SARUS_MPI_HOOK", "1"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // SSH hook enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.enableSSH = true;
        config->commandRun.hostEnvironment = {};
        metadata.env = {};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{
            {"SARUS_SSH_HOOK", "1"},
            {"SARUS_SLURM_GLOBAL_SYNC_HOOK", "1"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
}

TEST(ConfigsMergerTestGroup, command_to_execute) {
    // only CLI cmd
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
        auto metadata = common::ImageMetadata{};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"cmd-cli"}));
    }
    // only metadata cmd
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.execArgs = common::CLIArguments{};
        auto metadata = common::ImageMetadata{};
        metadata.cmd = common::CLIArguments{"cmd-metadata"};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"cmd-metadata"}));
    }
    // CLI cmd overrides metadata cmd
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
        auto metadata = common::ImageMetadata{};
        metadata.cmd = common::CLIArguments{"cmd-metadata"};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"cmd-cli"}));
    }
    // only CLI entrypoint
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.entrypoint = common::CLIArguments{"entry-cli"};
        auto metadata = common::ImageMetadata{};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-cli"}));
    }
    // only metadata entrypoint
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        auto metadata = common::ImageMetadata{};
        metadata.entry = common::CLIArguments{"entry-metadata"};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-metadata"}));
    }
    // entrypoint + cmd
    {
        // metadata entrypoint + metadata cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            auto metadata = common::ImageMetadata{};
            metadata.cmd = common::CLIArguments{"cmd-metadata"};
            metadata.entry = common::CLIArguments{"entry-metadata"};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-metadata", "cmd-metadata"}));
        }
        // CLI entrypoint + CLI cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
            config->commandRun.entrypoint = common::CLIArguments{"entry-cli"};
            auto metadata = common::ImageMetadata{};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-cli", "cmd-cli"}));
        }
        // metadata entrypoint + CLI cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
            auto metadata = common::ImageMetadata{};
            metadata.entry = common::CLIArguments{"entry-metadata"};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-metadata", "cmd-cli"}));
        }
        // CLI entrypoint overrides metadata entrypoint and metadata cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            config->commandRun.entrypoint = common::CLIArguments{"entry-cli"};
            auto metadata = common::ImageMetadata{};
            metadata.cmd = common::CLIArguments{"cmd-metadata"};
            metadata.entry = common::CLIArguments{"entry-metadata"};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-cli"}));
        }
    }
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
