/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "DeviceParser.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "common/Error.hpp"
#include "cli/Utility.hpp"


namespace sarus {
namespace cli {

DeviceParser::DeviceParser(std::shared_ptr<const common::Config> conf)
    : conf{std::move(conf)}
{}


std::unique_ptr<runtime::DeviceMount> DeviceParser::parseDeviceRequest(const std::string& requestString) const {
    auto message = boost::format("Parsing device request '%s'") % requestString;
    utility::printLog(message, common::LogLevel::DEBUG);

    if (requestString.empty()) {
        auto message = boost::format("Invalid device request: no values provided");
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    auto requestVector = std::vector<std::string>{};
    boost::split(requestVector, requestString, boost::is_any_of(":"));

    if (requestVector.size() > 3) {
        auto message = boost::format("Invalid device request '%s': too many tokens provided. "
            "The format of the option value must be at most '<host device>:<container device>:<access>'")
            % requestString;
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    auto source = boost::filesystem::path{requestVector[0]};
    auto destination = source;
    auto accessString = std::string{"rwm"};

    if (requestVector.size() == 3) {
        destination = requestVector[1];
        accessString = requestVector[2];
    }
    else if (requestVector.size() == 2) {
        if (boost::filesystem::path{requestVector[1]}.is_relative()) {
            accessString = requestVector[1];
        }
        else {
            destination = requestVector[1];
        }
    }

    unsigned long flags = 0;
    flags |= MS_REC;
    flags |= MS_PRIVATE;

    try {
        validateMountPath(source, "host");
        validateMountPath(destination, "container");
        auto deviceAccess = createDeviceAccess(accessString);
        return std::unique_ptr<runtime::DeviceMount>{new runtime::DeviceMount{source, destination, flags, deviceAccess, conf}};
    }
    catch (const common::Error& e) {
        auto message = boost::format("Invalid device request '%s': %s") % requestString %  e.getErrorTrace().back().errorMessage;
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_RETHROW_ERROR(e, message.str(), common::LogLevel::INFO);
    }
}

common::DeviceAccess DeviceParser::createDeviceAccess(const std::string& accessString) const {
    try {
        auto deviceAccess = common::DeviceAccess{accessString};
        return deviceAccess;
    }
    catch (const common::Error& e) {
        auto message = boost::format("%s. "
            "Device access must be entered as a combination of 'rwm' characters, with no repetitions")
            %  e.what();
        SARUS_RETHROW_ERROR(e, message.str(), common::LogLevel::INFO);
    }
}

void DeviceParser::validateMountPath(const boost::filesystem::path& path, const std::string& context) const {
    if (path.empty()) {
        auto message = boost::format("detected empty %s device path") % context;
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    if (path.is_relative()) {
        auto message = boost::format("%s device path '%s' must be absolute") % context % path.string();
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
}

} // namespace
} // namespace
