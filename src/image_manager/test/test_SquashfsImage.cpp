/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Utility.hpp"
#include "common/PathRAII.hpp"
#include "image_manager/SquashfsImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(SquashfsImageTestGroup) {
};

TEST(SquashfsImageTestGroup, testSquashfsImage) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = *configRAII.config;
    config.imageReference = {"server", "repositoryNamespace", "image", "tag"};

    common::PathRAII repository{config.directories.repository};
    boost::filesystem::remove_all(repository.getPath());

    common::PathRAII unpackedImage{common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-unpackedImage")};
    common::createFoldersIfNecessary(unpackedImage.getPath());

    SquashfsImage{config, unpackedImage.getPath(), config.getImageFile()};

    CHECK(boost::filesystem::exists(config.getImageFile()));
}

TEST(SquashfsImageTestGroup, testGenerateMksquashfsArgs) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto expectedMksquashfsPath = config->json["mksquashfsPath"].GetString();

    auto sourcePath = std::string{"/tmp/test-source-image"};
    auto destinationPath = std::string{"/tmp/test-destination-image"};

    // Options as defined in config generated by test_utility
    auto generatedArgs = SquashfsImage::generateMksquashfsArgs(*config, sourcePath, destinationPath);
    auto expectedArgs = common::CLIArguments{expectedMksquashfsPath, sourcePath, destinationPath, "-comp gzip -Xcompression-level 6"};
    CHECK(generatedArgs == expectedArgs);

    // Options not present in config
    config->json.RemoveMember("mksquashfsOptions");
    generatedArgs = SquashfsImage::generateMksquashfsArgs(*config, sourcePath, destinationPath);
    expectedArgs = common::CLIArguments{expectedMksquashfsPath, sourcePath, destinationPath};
    CHECK(generatedArgs == expectedArgs);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
