/********************************************************************************************
* Supersingular Isogeny Key Encapsulation Library
*
* Abstract: supersingular isogeny key encapsulation (SIKE) protocol
*********************************************************************************************/

#include <string.h>
#include "P503_internal_r1.h"
#include "fips202_r1.h"
#include "pq-crypto/s2n_pq_random.h"
#include "utils/s2n_safety.h"
#include "tls/s2n_kem.h"
#include "pq-crypto/s2n_pq.h"

int SIKE_P503_r1_crypto_kem_keypair(unsigned char *pk, unsigned char *sk)
{ // SIKE's key generation
  // Outputs: secret key sk (SIKE_P503_R1_SECRET_KEY_BYTES = MSG_BYTES + SECRETKEY_B_BYTES + SIKE_P503_R1_PUBLIC_KEY_BYTES bytes)
  //          public key pk (SIKE_P503_R1_PUBLIC_KEY_BYTES bytes)
    POSIX_ENSURE(s2n_pq_is_enabled(), S2N_ERR_PQ_DISABLED);

    digit_t _sk[SECRETKEY_B_BYTES/sizeof(digit_t)];

    // Generate lower portion of secret key sk <- s||SK
    POSIX_GUARD_RESULT(s2n_get_random_bytes(sk, MSG_BYTES));
    POSIX_GUARD(random_mod_order_B((unsigned char*)_sk));

    // Generate public key pk
    EphemeralKeyGeneration_B(_sk, pk);

    memcpy(sk + MSG_BYTES, _sk, SECRETKEY_B_BYTES);
    // Append public key pk to secret key sk
    memcpy(&sk[MSG_BYTES + SECRETKEY_B_BYTES], pk, SIKE_P503_R1_PUBLIC_KEY_BYTES);

    return 0;
}


int SIKE_P503_r1_crypto_kem_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{ // SIKE's encapsulation
  // Input:   public key pk         (SIKE_P503_R1_PUBLIC_KEY_BYTES bytes)
  // Outputs: shared secret ss      (SIKE_P503_R1_SHARED_SECRET_BYTES bytes)
  //          ciphertext message ct (SIKE_P503_R1_CIPHERTEXT_BYTES = SIKE_P503_R1_PUBLIC_KEY_BYTES + MSG_BYTES bytes)
    POSIX_ENSURE(s2n_pq_is_enabled(), S2N_ERR_PQ_DISABLED);

    const uint16_t G = 0;
    const uint16_t H = 1;
    const uint16_t P = 2;
    union {
        unsigned char b[SECRETKEY_A_BYTES];
        digit_t       d[SECRETKEY_A_BYTES/sizeof(digit_t)];
    } ephemeralsk;
    unsigned char jinvariant[FP2_ENCODED_BYTES];
    unsigned char h[MSG_BYTES];
    unsigned char temp[SIKE_P503_R1_CIPHERTEXT_BYTES+MSG_BYTES];
    unsigned int i;

    // Generate ephemeralsk <- G(m||pk) mod oA
    POSIX_GUARD_RESULT(s2n_get_random_bytes(temp, MSG_BYTES));
    memcpy(&temp[MSG_BYTES], pk, SIKE_P503_R1_PUBLIC_KEY_BYTES);
    cshake256_simple(ephemeralsk.b, SECRETKEY_A_BYTES, G, temp, SIKE_P503_R1_PUBLIC_KEY_BYTES+MSG_BYTES);

    /* ephemeralsk is a union; the memory set here through .b will get accessed through the .d member later */
    /* cppcheck-suppress unreadVariable */
    ephemeralsk.b[SECRETKEY_A_BYTES - 1] &= MASK_ALICE;

    // Encrypt
    EphemeralKeyGeneration_A(ephemeralsk.d, ct);
    EphemeralSecretAgreement_A(ephemeralsk.d, pk, jinvariant);
    cshake256_simple(h, MSG_BYTES, P, jinvariant, FP2_ENCODED_BYTES);
    for (i = 0; i < MSG_BYTES; i++) ct[i + SIKE_P503_R1_PUBLIC_KEY_BYTES] = temp[i] ^ h[i];

    // Generate shared secret ss <- H(m||ct)
    memcpy(&temp[MSG_BYTES], ct, SIKE_P503_R1_CIPHERTEXT_BYTES);
    cshake256_simple(ss, SIKE_P503_R1_SHARED_SECRET_BYTES, H, temp, SIKE_P503_R1_CIPHERTEXT_BYTES+MSG_BYTES);

    return 0;
}


int SIKE_P503_r1_crypto_kem_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{ // SIKE's decapsulation
  // Input:   secret key sk         (SIKE_P503_R1_SECRET_KEY_BYTES = MSG_BYTES + SECRETKEY_B_BYTES + SIKE_P503_R1_PUBLIC_KEY_BYTES bytes)
  //          ciphertext message ct (SIKE_P503_R1_CIPHERTEXT_BYTES = SIKE_P503_R1_PUBLIC_KEY_BYTES + MSG_BYTES bytes)
  // Outputs: shared secret ss      (SIKE_P503_R1_SHARED_SECRET_BYTES bytes)
    POSIX_ENSURE(s2n_pq_is_enabled(), S2N_ERR_PQ_DISABLED);

    const uint16_t G = 0;
    const uint16_t H = 1;
    const uint16_t P = 2;
    union {
        unsigned char b[SECRETKEY_A_BYTES];
        digit_t       d[SECRETKEY_A_BYTES/sizeof(digit_t)];
    } ephemeralsk_;
    unsigned char jinvariant_[FP2_ENCODED_BYTES];
    unsigned char h_[MSG_BYTES];
    unsigned char c0_[SIKE_P503_R1_PUBLIC_KEY_BYTES];
    unsigned char temp[SIKE_P503_R1_CIPHERTEXT_BYTES+MSG_BYTES];
    unsigned int i;
    bool dont_copy = 1;

    digit_t _sk[SECRETKEY_B_BYTES/sizeof(digit_t)];

	memcpy(_sk, sk + MSG_BYTES, SECRETKEY_B_BYTES);

    // Decrypt
    if (!(EphemeralSecretAgreement_B(_sk, ct, jinvariant_) == 0)) {
        goto S2N_SIKE_P503_R1_HASHING;
    }
    cshake256_simple(h_, MSG_BYTES, P, jinvariant_, FP2_ENCODED_BYTES);
    for (i = 0; i < MSG_BYTES; i++) temp[i] = ct[i + SIKE_P503_R1_PUBLIC_KEY_BYTES] ^ h_[i];

    // Generate ephemeralsk_ <- G(m||pk) mod oA
    memcpy(&temp[MSG_BYTES], &sk[MSG_BYTES + SECRETKEY_B_BYTES], SIKE_P503_R1_PUBLIC_KEY_BYTES);
    cshake256_simple(ephemeralsk_.b, SECRETKEY_A_BYTES, G, temp, SIKE_P503_R1_PUBLIC_KEY_BYTES+MSG_BYTES);

    /* ephemeralsk_ is a union; the memory set here through .b will get accessed through the .d member later */
    /* cppcheck-suppress unreadVariable */
    /* cppcheck-suppress uninitvar */
    ephemeralsk_.b[SECRETKEY_A_BYTES - 1] &= MASK_ALICE;

    // Generate shared secret ss <- H(m||ct) or output ss <- H(s||ct)
    EphemeralKeyGeneration_A(ephemeralsk_.d, c0_);

    // Note: This step deviates from the NIST supplied code by using constant time operations.
    // We only want to copy the data if c0_ and ct are different
    dont_copy = s2n_constant_time_equals(c0_, ct, SIKE_P503_R1_PUBLIC_KEY_BYTES);
S2N_SIKE_P503_R1_HASHING:
    // The last argument to s2n_constant_time_copy_or_dont is dont and thus prevents the copy when non-zero/true
    s2n_constant_time_copy_or_dont(temp, sk, MSG_BYTES, dont_copy);

    memcpy(&temp[MSG_BYTES], ct, SIKE_P503_R1_CIPHERTEXT_BYTES);
    cshake256_simple(ss, SIKE_P503_R1_SHARED_SECRET_BYTES, H, temp, SIKE_P503_R1_CIPHERTEXT_BYTES+MSG_BYTES);

    return 0;
}
