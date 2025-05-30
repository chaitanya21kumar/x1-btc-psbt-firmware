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

#include "atecc.h"
#include "hardfault.h"
#include "securechip/securechip.h"
#include <i2c_ecc.h>
#include <salt.h>
#include <util.h>
#include <wally_crypto.h>

// disabling some warnings, as it's an external library.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cryptoauthlib.h>
#include <host/atca_host.h>
#pragma GCC diagnostic pop

typedef enum {
    ATECC_SLOT_IO_PROTECTION_KEY = 0,
    ATECC_SLOT_AUTHKEY = 1,
    ATECC_SLOT_ENCRYPTION_KEY = 2,
    ATECC_SLOT_ROLLKEY = 3,
    ATECC_SLOT_KDF = 4,
    ATECC_SLOT_ATTESTATION = 5,
    // Deprecated as the equivalent does not exist in the Optiga chip.
    ATECC_SLOT_ECC_UNSAFE_SIGN_DEPRECATED = 6,
    ATECC_SLOT_DATA0 = 9,
    // The other slots are currently not in use.
} atecc_slot_t;

// Chip Configuration, generated with "make generate-atecc608-config"
// The first 16 bytes, as well as the LockValue/LockConfig can't be changed and are ignored when
// writing the configuration to the device. Locking is performed via the Lock command during setup,
// after writing the configuration.
// UserExtra and UserExtraAdd are setup automatically via the UpdateExtra command based on this
// configuration.
// The Counter0/Counter1 values are overwritten at setup via atcab_write_config_counter().
// Individual slot locking is performed at setup via atcab_lock_data_slot().
#if (ATCA_ECC_CONFIG_SIZE != 128)
#error "Unexpected configuration size"
#endif
// clang-format off
static uint8_t _configuration[ATCA_ECC_CONFIG_SIZE] = {
    0x01, 0x23, 0x68, 0xee, 0x00, 0x00, 0x60, 0x02,
    0x8a, 0x1d, 0xde, 0x66, 0xee, 0x01, 0x01, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0xa0, 0x20, 0x80, 0x42, 0x83, 0x20,
    0x83, 0x62, 0x83, 0x20, 0xc2, 0x42, 0xc2, 0x42,
    0xc2, 0x42, 0xc2, 0x42, 0xc2, 0x42, 0xc2, 0x42,
    0xc2, 0x42, 0xc2, 0x42, 0x01, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x7c, 0x00, 0x7c, 0x00, 0xfc, 0x01, 0xdc, 0x01,
    0xdc, 0x01, 0xf3, 0x01, 0xd3, 0x01, 0xd3, 0x01,
    0xdc, 0x01, 0xdc, 0x01, 0xdc, 0x01, 0xdc, 0x01,
    0xdc, 0x01, 0xdc, 0x01, 0xdc, 0x01, 0xdc, 0x01,
};
// clang-format on

// Number of times the first kdf slot can be used.
#define MONOTONIC_COUNTER_MAX_USE (730500)

// This number of KDF iterations on the 2nd kdf slot when stretching the device
// password.
#define KDF_NUM_ITERATIONS (2)

// The total individual size of the public key data slots (slots 9-15) is 72 bytes. Using encrypted
// read/write it is only possible to transmit 32 bytes. The last block is therefore 8 (72 =
// 32+32+8).
#define DATA_PUBLIC_KEY_SLOT_BLOCK_SIZE 32

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpacked"
#pragma GCC diagnostic ignored "-Wattributes"
typedef union {
    struct __attribute__((__packed__)) {
        uint32_t u2f_counter;
    } fields;
    uint8_t bytes[DATA_PUBLIC_KEY_SLOT_BLOCK_SIZE];
} data_9_0_t;

#pragma GCC diagnostic pop

static const securechip_interface_functions_t* _interface_functions = NULL;

/** \brief initialize an I2C interface using given config.
 * \param[in] hal - opaque ptr to HAL data
 * \param[in] cfg - interface configuration
 * \return ATCA_SUCCESS.
 */
static ATCA_STATUS _init(void* hal, void* cfg)
{
    (void)hal;
    (void)cfg;
    // store opaque data in ((ATCAHAL_t*)hal)->hal_data
    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C post init
 * \param[in] iface  instance
 * \return ATCA_SUCCESS
 */
static ATCA_STATUS _post_init(void* iface)
{
    (void)iface;
    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C receive function for ASF I2C
 * \param[in]    iface     Device to interact with.
 * \param[out]   rxdata    Data received will be returned here.
 * \param[inout] rxlength  As input, the size of the rxdata buffer.
 *                         As output, the number of bytes received.
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
static ATCA_STATUS _receive(void* iface, uint8_t word_address, uint8_t* rxdata, uint16_t* rxlength)
{
    (void)iface;
    (void)word_address;
    uint8_t ret = i2c_ecc_read(rxdata, *rxlength);
    if (ret) {
        return ATCA_COMM_FAIL;
    }
    *rxlength = rxdata[0];
    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C send over ASF
 * \param[in] iface     instance
 * \param[in] txdata    pointer to space to bytes to send
 * \param[in] txlength  number of bytes to send
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
static ATCA_STATUS _send(void* iface, uint8_t word_address, uint8_t* txdata, int txlength)
{
    (void)iface;
    (void)word_address;
    // txdata[0] is using _reserved byte of the ATCAPacket
    txdata[0] = I2C_ECC_CHIP_CMD;
    // Account for the _reserved byte, similar to
    // https://github.com/MicrochipTech/cryptoauthlib/blob/411cd5cfc314a875794a8fb5a4b6a1860384ec1b/lib/hal/hal_i2c_start.c#L312
    txlength++;
    return i2c_ecc_write(txdata, txlength) ? ATCA_COMM_FAIL : ATCA_SUCCESS;
}

/** \brief sleep CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to sleep
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
static ATCA_STATUS _sleep(void* iface)
{
    (void)iface;
    return i2c_ecc_sleep() ? ATCA_COMM_FAIL : ATCA_SUCCESS;
}

/** \brief wake up CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to wakeup
 * \return ATCA_SUCCESS on success, otherwise an error code.

 */
static ATCA_STATUS _wake(void* iface)
{
    (void)iface;
    return i2c_ecc_wake() == I2C_ECC_WAKE ? ATCA_SUCCESS : ATCA_COMM_FAIL;
}

/** \brief idle CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to idle
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
static ATCA_STATUS _idle(void* iface)
{
    (void)iface;
    return i2c_ecc_idle() ? ATCA_COMM_FAIL : ATCA_SUCCESS;
}

/** \brief cleanup resources created in _init().
 * \param[in] hal_data - opaque pointer to hal data structure.
 * \return ATCA_SUCCESS
 */
static ATCA_STATUS _release(void* hal_data)
{
    (void)hal_data;
    return ATCA_SUCCESS;
}

// set up custom i2c communication interface with cryptoauthlib.
static ATCAIfaceCfg cfg = {
    // TODO: can likely use cryptoauthlib/lib/hal/hal_i2c_start.(c|h) for all or
    // some of the functionality, possibly using cfg_ateccx08a_i2c_default
    .iface_type = ATCA_CUSTOM_IFACE,
    .devtype = ATECC608,
    .atcacustom.halinit = &_init,
    .atcacustom.halpostinit = &_post_init,
    .atcacustom.halreceive = &_receive,
    .atcacustom.halsend = &_send,
    .atcacustom.halsleep = &_sleep,
    .atcacustom.halwake = &_wake,
    .atcacustom.halidle = &_idle,
    .atcacustom.halrelease = &_release,
    .wake_delay = I2C_ECC_TWHI,
    .rx_retries = I2C_ECC_RETRIES,
    .cfg_data = NULL};

#if FACTORYSETUP == 1
/**
 * Individually locks a slot. Used to lock the io protection and auth key so
 * they can never change.
 */
static ATCA_STATUS _lock_slot(atecc_slot_t slot)
{
    bool is_locked = false;
    ATCA_STATUS result = atcab_is_slot_locked(slot, &is_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_locked) {
        return atcab_lock_data_slot(slot);
    }
    return ATCA_SUCCESS;
}

static ATCA_STATUS _factory_setup(void)
{
    if (_interface_functions == NULL) {
        return (ATCA_STATUS)SC_ERR_IFS;
    }
    bool is_config_locked = false;
    ATCA_STATUS result = atcab_is_locked(LOCK_ZONE_CONFIG, &is_config_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_config_locked) {
        // The chip is unlocked: configure the chip and lock it. This happens
        // only once.

        result = atcab_write_config_zone(_configuration);
        if (result != ATCA_SUCCESS) {
            return result;
        }

        // Set Counter0 so that it can be incremented
        // `MONOTONIC_COUNTER_MAX_USE` times. This counter is attached to slots
        // with limited use.
        result = atcab_write_config_counter(0, COUNTER_MAX_VALUE - MONOTONIC_COUNTER_MAX_USE);
        if (result != ATCA_SUCCESS) {
            return result;
        }
        // Set Counter1 to 0.
        result = atcab_write_config_counter(1, 0);
        if (result != ATCA_SUCCESS) {
            return result;
        }

        result = atcab_lock_config_zone();
        if (result != ATCA_SUCCESS) {
            return result;
        }
        is_config_locked = true;
    }
    bool is_data_locked;
    result = atcab_is_locked(LOCK_ZONE_DATA, &is_data_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (is_config_locked && !is_data_locked) {
        // Write IO protection key.
        uint8_t io_protection_key[32] = {0};
        UTIL_CLEANUP_32(io_protection_key);
        _interface_functions->get_io_protection_key(io_protection_key);
        result = atcab_write_zone(
            ATCA_ZONE_DATA, ATECC_SLOT_IO_PROTECTION_KEY, 0, 0, io_protection_key, 32);
        if (result != ATCA_SUCCESS) {
            return result;
        }
        // Write auth key.
        uint8_t auth_key[32] = {0};
        UTIL_CLEANUP_32(auth_key);
        _interface_functions->get_auth_key(auth_key);
        result = atcab_write_zone(ATCA_ZONE_DATA, ATECC_SLOT_AUTHKEY, 0, 0, auth_key, 32);
        if (result != ATCA_SUCCESS) {
            return result;
        }
        // Write encryption key.
        uint8_t encryption_key[32] = {0};
        UTIL_CLEANUP_32(encryption_key);
        _interface_functions->get_encryption_key(encryption_key);
        result =
            atcab_write_zone(ATCA_ZONE_DATA, ATECC_SLOT_ENCRYPTION_KEY, 0, 0, encryption_key, 32);
        if (result != ATCA_SUCCESS) {
            return result;
        }
        result = atcab_lock_data_zone();
        if (result != ATCA_SUCCESS) {
            return result;
        }
        is_data_locked = true;
    }
    if (is_config_locked && is_data_locked) {
        result = _lock_slot(ATECC_SLOT_IO_PROTECTION_KEY);
        if (result != ATCA_SUCCESS) {
            return result;
        }
        result = _lock_slot(ATECC_SLOT_AUTHKEY);
        if (result != ATCA_SUCCESS) {
            return result;
        }
        result = _lock_slot(ATECC_SLOT_ENCRYPTION_KEY);
        if (result != ATCA_SUCCESS) {
            return result;
        }
    }
    return ATCA_SUCCESS;
}
#endif

static int _verify_config(void)
{
    bool is_locked;
    ATCA_STATUS result;

    // Check that the config and data zones are locked.
    result = atcab_is_locked(LOCK_ZONE_CONFIG, &is_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_locked) {
        return SC_ATECC_ERR_ZONE_UNLOCKED_CONFIG;
    }
    result = atcab_is_locked(LOCK_ZONE_DATA, &is_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_locked) {
        return SC_ATECC_ERR_ZONE_UNLOCKED_DATA;
    }

    bool same_config = false;
    result = atcab_cmp_config_zone(_configuration, &same_config);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!same_config) {
        return SC_ERR_CONFIG_MISMATCH;
    }

    // Check that the slots are individually locked.
    result = atcab_is_slot_locked(ATECC_SLOT_IO_PROTECTION_KEY, &is_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_locked) {
        return SC_ATECC_ERR_SLOT_UNLOCKED_IO;
    }
    result = atcab_is_slot_locked(ATECC_SLOT_AUTHKEY, &is_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_locked) {
        return SC_ATECC_ERR_SLOT_UNLOCKED_AUTH;
    }
    result = atcab_is_slot_locked(ATECC_SLOT_ENCRYPTION_KEY, &is_locked);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    if (!is_locked) {
        return SC_ATECC_ERR_SLOT_UNLOCKED_ENC;
    }
    return ATCA_SUCCESS;
}

int atecc_setup(const securechip_interface_functions_t* ifs)
{
    if (ifs == NULL) {
        return SC_ERR_IFS;
    }
    _interface_functions = ifs;
    ATCA_STATUS result = atcab_init(&cfg);
    if (result != ATCA_SUCCESS) {
        return result;
    }

#if FACTORYSETUP == 1
    result = _factory_setup();
    if (result != ATCA_SUCCESS) {
        return result;
    }
#endif

    return _verify_config();
}

/**
 * This performs the CheckMac command on ATECC_SLOT_AUTHKEY. This needs to
 * be called before using any slot requiring auth and whose KeyConfig.AuthKey is
 * ATECC_SLOT_AUTHKEY.
 */
static ATCA_STATUS _authorize_key(void)
{
    uint8_t num_in[NONCE_NUMIN_SIZE] = {0};
    uint8_t rand_out[32] = {0};

    atca_temp_key_t temp_key = {0};

    atca_nonce_in_out_t nonce_params = {
        .mode = NONCE_MODE_SEED_UPDATE,
        .zero = 0,
        .num_in = num_in,
        .rand_out = rand_out,
        .temp_key = &temp_key,
    };
    ATCA_STATUS result = atcab_nonce_rand(nonce_params.num_in, rand_out);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    // Calculate contents of TempKey.
    result = atcah_nonce(&nonce_params);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    uint8_t response[32] = {0};
    const uint8_t other_data[13] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t sn[9] = {0};
    result = atcab_read_serial_number(sn);
    if (result != ATCA_SUCCESS) {
        return result;
    }

    uint8_t auth_key[32] = {0};
    UTIL_CLEANUP_32(auth_key);
    _interface_functions->get_auth_key(auth_key);
    atca_check_mac_in_out_t checkmac_params = {
        // First SHA block from slot key, Second SHA block from TempKey.
        .mode = CHECKMAC_MODE_BLOCK2_TEMPKEY,
        .key_id = ATECC_SLOT_AUTHKEY,
        .sn = sn,
        .client_chal = NULL, // unused in this mode
        .client_resp = response,
        .other_data = other_data,
        .otp = NULL, // unused in this mode,
        .slot_key = auth_key,
        .target_key = NULL,
        .temp_key = &temp_key,
    };
    // Compute client response.
    result = atcah_check_mac(&checkmac_params);
    if (result != ATCA_SUCCESS) {
        return result;
    }

    return atcab_checkmac(
        checkmac_params.mode,
        checkmac_params.key_id,
        checkmac_params.client_chal,
        checkmac_params.client_resp,
        checkmac_params.other_data);
}

/**
 * Performs a roll-key operation on a ATECC_SLOT_ROLLKEY.
 * @return ATCA_SUCCESS on success.
 */
static ATCA_STATUS _rollkey(void)
{
    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return result;
    }

    uint8_t num_in[NONCE_NUMIN_SIZE] = {0};
    result = atcab_nonce_rand(num_in, NULL);
    if (result != ATCA_SUCCESS) {
        return result;
    }
    return atcab_derivekey(0, ATECC_SLOT_ROLLKEY, NULL);
}

/**
 * Writes a new random key to ATECC_SLOT_KDF.
 * @return ATCA_SUCCESS on success.
 */
static ATCA_STATUS _update_kdf_key(void)
{
    uint8_t new_key[32] = {0};
    UTIL_CLEANUP_32(new_key);
    _interface_functions->random_32_bytes(new_key);
    uint8_t encryption_key[32] = {0};
    UTIL_CLEANUP_32(encryption_key);
    _interface_functions->get_encryption_key(encryption_key);

    uint8_t nonce_contribution[32] = {0};
    UTIL_CLEANUP_32(nonce_contribution);
    _interface_functions->random_32_bytes(nonce_contribution);
#if NONCE_NUMIN_SIZE > 32
#error "size mismatch"
#endif

    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return result;
    }

    return atcab_write_enc(
        ATECC_SLOT_KDF, 0, new_key, encryption_key, ATECC_SLOT_ENCRYPTION_KEY, nonce_contribution);
}

static int _atecc_kdf(atecc_slot_t slot, const uint8_t* msg, size_t len, uint8_t* kdf_out)
{
    if (len > 127 || (slot != ATECC_SLOT_ROLLKEY && slot != ATECC_SLOT_KDF)) {
        return SC_ERR_INVALID_ARGS;
    }
    if (msg == kdf_out) {
        return SC_ERR_INVALID_ARGS;
    }

    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return result;
    }

    uint8_t nonce_out[32] = {0};

    // The result is hkdf_extract with the msg as ikm (input key material) and
    // the slot key as the salt. hkdf info does not apply, as it is part of
    // hkdf_expand, which is not performed. hkdf_extract is simply hmac.
    // Python equivalent:
    // import hmac, hashlib; hmac.new(slot_key, msg, hashlib.sha256).digest()
    result = atcab_kdf(
        KDF_MODE_SOURCE_SLOT | KDF_MODE_TARGET_OUTPUT_ENC | KDF_MODE_ALG_HKDF,
        slot,
        KDF_DETAILS_HKDF_MSG_LOC_INPUT | (len << 24), // << 24, not << 25 as
                                                      // described in the data
                                                      // sheet.
        msg,
        kdf_out,
        nonce_out);

    // For PRF instead of HKDF, the Python equivalent is (msg = label+seed):
    // from scapy.layers.tls.crypto.prf import _tls12_SHA256PRF
    // _tls12_SHA256PRF(slot_key, msg, '', 32)
    /* result = atcab_kdf( */
    /*     KDF_MODE_SOURCE_SLOT | KDF_MODE_TARGET_OUTPUT | KDF_MODE_ALG_PRF, */
    /*     slot, */
    /*     KDF_DETAILS_PRF_KEY_LEN_32 | (len << 24), */
    /*     msg, */
    /*     kdf_out, */
    /*     nonce_out); */
    if (result != ATCA_SUCCESS) {
        return result;
    }
    // Output is encrypted with the io protection key.
    uint8_t io_protection_key[32] = {0};
    UTIL_CLEANUP_32(io_protection_key);
    _interface_functions->get_io_protection_key(io_protection_key);
    atca_io_decrypt_in_out_t io_dec_params = {
        .io_key = io_protection_key,
        .out_nonce = nonce_out,
        .data = kdf_out,
        .data_size = 32,
    };
    return atcah_io_decrypt(&io_dec_params);
}

int atecc_kdf(const uint8_t* msg, size_t len, uint8_t* kdf_out)
{
    return _atecc_kdf(ATECC_SLOT_KDF, msg, len, kdf_out);
}

int atecc_init_new_password(const char* password)
{
    (void)password;
    if (!atecc_reset_keys()) {
        return SC_ATECC_ERR_RESET_KEYS;
    }
    return 0;
}

int atecc_stretch_password(const char* password, uint8_t* stretched_out)
{
    uint8_t password_salted_hashed[32] = {0};
    UTIL_CLEANUP_32(password_salted_hashed);
    if (!salt_hash_data(
            (const uint8_t*)password,
            strlen(password),
            "keystore_seed_access_in",
            password_salted_hashed)) {
        return SC_ERR_SALT;
    }

    uint8_t kdf_in[32] = {0};
    UTIL_CLEANUP_32(kdf_in);
    memcpy(kdf_in, password_salted_hashed, 32);

    // First KDF on rollkey increments the monotonic counter. Call only once!
    int securechip_result = _atecc_kdf(ATECC_SLOT_ROLLKEY, kdf_in, 32, stretched_out);
    if (securechip_result) {
        return securechip_result;
    }
    // Second KDF does not use the counter and we call it multiple times.
    for (int i = 0; i < KDF_NUM_ITERATIONS; i++) {
        memcpy(kdf_in, stretched_out, 32);
        securechip_result = securechip_kdf(kdf_in, 32, stretched_out);
        if (securechip_result) {
            return securechip_result;
        }
    }

    if (!salt_hash_data(
            (const uint8_t*)password,
            strlen(password),
            "keystore_seed_access_out",
            password_salted_hashed)) {
        return SC_ERR_SALT;
    }
    if (wally_hmac_sha256(
            password_salted_hashed,
            sizeof(password_salted_hashed),
            stretched_out,
            32,
            stretched_out,
            32) != WALLY_OK) {
        return SC_ERR_HASH;
    }
    return 0;
}

bool atecc_reset_keys(void)
{
    if (_rollkey() != ATCA_SUCCESS) {
        return false;
    }
    return _update_kdf_key() == ATCA_SUCCESS;
}

bool atecc_gen_attestation_key(uint8_t* pubkey_out)
{
    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return false;
    }
    return atcab_genkey(ATECC_SLOT_ATTESTATION, pubkey_out) == ATCA_SUCCESS;
}

bool atecc_attestation_sign(const uint8_t* msg, uint8_t* signature_out)
{
    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return false;
    }
    return atcab_sign(ATECC_SLOT_ATTESTATION, msg, signature_out) == ATCA_SUCCESS;
}

bool atecc_monotonic_increments_remaining(uint32_t* remaining_out)
{
    uint32_t counter;
    if (atcab_counter_read(0, &counter) != ATCA_SUCCESS) {
        return false;
    }
    if (COUNTER_MAX_VALUE < counter) {
        Abort("ATECC returned an invalid value");
    }
    *remaining_out = COUNTER_MAX_VALUE - counter;
    return true;
}

bool atecc_random(uint8_t* rand_out)
{
    for (int retries = 0; retries < 5; retries++) {
        if (atcab_random(rand_out) == ATCA_SUCCESS) {
            return true;
        }
    }
    return false;
}

#if APP_U2F == 1 || FACTORYSETUP == 1
// Read a "standard" sized block from a data slot (must be 32 bytes)
static bool _read_data_slot_block(uint8_t* bytes, uint16_t slot, uint8_t block)
{
    uint8_t encryption_key[32] = {0};
    UTIL_CLEANUP_32(encryption_key);
    _interface_functions->get_encryption_key(encryption_key);

    uint8_t nonce_contribution[32] = {0};
    UTIL_CLEANUP_32(nonce_contribution);
    _interface_functions->random_32_bytes(nonce_contribution);
#if NONCE_NUMIN_SIZE > 32
#error "size mismatch"
#endif

    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return false;
    }
    return atcab_read_enc(
               slot, block, bytes, encryption_key, ATECC_SLOT_ENCRYPTION_KEY, nonce_contribution) ==
           ATCA_SUCCESS;
}

// Write a "standard" sized block from a data slot (must be 32 bytes)
static bool _write_data_slot_block(uint8_t* bytes, uint16_t slot, uint8_t block)
{
    uint8_t encryption_key[32] = {0};
    UTIL_CLEANUP_32(encryption_key);
    _interface_functions->get_encryption_key(encryption_key);

    uint8_t nonce_contribution[32] = {0};
    UTIL_CLEANUP_32(nonce_contribution);
    _interface_functions->random_32_bytes(nonce_contribution);
#if NONCE_NUMIN_SIZE > 32
#error "size mismatch"
#endif

    ATCA_STATUS result = _authorize_key();
    if (result != ATCA_SUCCESS) {
        return false;
    }
    result = atcab_write_enc(
        slot, block, bytes, encryption_key, ATECC_SLOT_ENCRYPTION_KEY, nonce_contribution);
    if (result != ATCA_SUCCESS) {
        return false;
    }
    // Double-check by reading it back and comparing.
    uint8_t written_bytes[32] = {0};
    if (!_read_data_slot_block(written_bytes, slot, block)) {
        return false;
    }
    return MEMEQ(written_bytes, bytes, sizeof(written_bytes));
}

bool atecc_u2f_counter_set(uint32_t counter)
{
    data_9_0_t data = {0};
    if (!_read_data_slot_block(&data.bytes[0], ATECC_SLOT_DATA0, 0)) {
        return false;
    }

    data.fields.u2f_counter = counter;

    return _write_data_slot_block(&data.bytes[0], ATECC_SLOT_DATA0, 0);
}
#endif

#if APP_U2F == 1
bool atecc_u2f_counter_inc(uint32_t* counter)
{
    data_9_0_t data = {0};
    if (!_read_data_slot_block(&data.bytes[0], ATECC_SLOT_DATA0, 0)) {
        return false;
    }

    data.fields.u2f_counter += 1;
    *counter = data.fields.u2f_counter;

    return _write_data_slot_block(&data.bytes[0], ATECC_SLOT_DATA0, 0);
}
#endif

bool atecc_model(securechip_model_t* model_out)
{
    uint8_t revision[4] = {0};
    if (atcab_info(revision) != ATCA_SUCCESS) {
        return false;
    }
    *model_out = revision[3] >= 0x03 ? ATECC_ATECC608B : ATECC_ATECC608A;
    return true;
}
