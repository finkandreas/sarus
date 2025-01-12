/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _SarusImage_hpp
#define _SarusImage_hpp

#include <string>
#include <boost/filesystem.hpp>

#include "ImageReference.hpp"


namespace sarus {
namespace common {

struct SarusImage {
    common::ImageReference reference;  // A reference can be expressed as a string in the format
                                       // "server/namespace/image[:tag][@digest]".
                                       // If the digest is present, it is the digest of the container
                                       // image manifest in the *registry* it was pulled from;
                                       // it is *NOT* the manifest digest of the OCI image pulled with Skopeo

    std::string id;              // The sha256 hash of the image configuration JSON,
                                 // as defined by the OCI Image specification:
                                 // https://github.com/opencontainers/image-spec/blob/main/config.md#imageid

    std::string datasize;        // The size of container image file

    std::string created;         // The time when the image was added to the Sarus local repository;
                                 // *NOT* the time when the image was originally built

    boost::filesystem::path imageFile;
    boost::filesystem::path metadataFile;

    static std::string createTimeString(time_t time_in);
    static std::string createSizeString(size_t size);
};

bool operator==(const SarusImage&, const SarusImage&);

}
}

#endif
