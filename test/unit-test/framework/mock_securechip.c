// Copyright 2019 Chaitanya Kumar
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <cmocka.h>

#include <salt.h>
#include <securechip/securechip.h>
#include <stdio.h>
#include <string.h>
#include <wally_crypto.h>

static uint32_t _u2f_counter;

// Mocked contents of the secure chip rollkey slot.
static const uint8_t _rollkey[32] =
    "\x9d\xd1\x34\x1f\x6b\x4b\x26\xb1\x72\x89\xa1\xa3\x92\x71\x5c\xf0\xd0\x57\x8c\x84\xdb\x9a\x51"
    "\xeb\xde\x14\x24\x06\x69\xd1\xd0\x5e";

// Mocked contents of the securechip kdf slot.
static const uint8_t _kdfkey[32] =
    "\xd2\xe1\xe6\xb1\x8b\x6c\x6b\x08\x43\x3e\xdb\xc1\xd1\x68\xc1\xa0\x04\x37\x74\xa4\x22\x18\x77"
    "\xe7\x9e\xd5\x66\x84\xbe\x5a\xc0\x1b";

int securechip_kdf(const uint8_t* msg, size_t len, uint8_t* kdf_out)
{
    wally_hmac_sha256(_kdfkey, 32, msg, len, kdf_out, 32);
    return 0;
}
int securechip_kdf_rollkey(const uint8_t* msg, size_t len, uint8_t* kdf_out)
{
    wally_hmac_sha256(_rollkey, 32, msg, len, kdf_out, 32);
    return 0;
}
int securechip_init_new_password(const char* password)
{
    (void)password;
    return 0;
}
int securechip_stretch_password(const char* password, uint8_t* stretched_out)
{
    uint8_t key[9] = "unit-test";
    wally_hmac_sha256(
        key, sizeof(key), (const uint8_t*)password, strlen(password), stretched_out, 32);
    return 0;
}
bool securechip_u2f_counter_set(uint32_t counter)
{
    _u2f_counter = counter;
    return true;
}

bool securechip_u2f_counter_inc(uint32_t* counter)
{
    *counter = _u2f_counter++;
    return true;
}

bool securechip_attestation_sign(const uint8_t* msg, uint8_t* signature_out)
{
    return false;
}

bool securechip_monotonic_increments_remaining(uint32_t* remaining_out)
{
    *remaining_out = 1;
    return true;
}

bool securechip_model(securechip_model_t* model_out)
{
    *model_out = ATECC_ATECC608B;
    return true;
}
