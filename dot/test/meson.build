# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

openssl_dep = dependency('openssl', required: true)

tests = [['nsmDotUtils_test', 'nsmDotUtils_test.cpp']]

foreach t : tests
    test(
        t[0],
        executable(
            t[0],
            t[1],
            '../nsmDotUtils.cpp',
            include_directories: ['..'],
            dependencies: [gtest, openssl_dep],
        ),
    )
endforeach
