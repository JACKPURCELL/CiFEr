/*
 * Copyright (c) 2018 XLAB d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sodium.h>
#include <amcl/pair_BN254.h>
#include <amcl/pbc_support.h>

#include "cifer/internal/big.h"
#include "cifer/innerprod/fullysec/dmcfe.h"
#include "cifer/internal/dlog.h"
#include "cifer/internal/hash.h"
#include "cifer/sample/uniform.h"

//for clients
void cfe_mcfe_server_init(cfe_dmcfe_client *c, size_t idx) {
    mpz_t client_sec_key;
    mpz_inits(c->order, client_sec_key, NULL);

    c->idx = idx;
    BIG_256_56_rcopy(c->order_big, CURVE_Order_BN254);
    mpz_from_BIG_256_56(c->order, c->order_big);
    cfe_vec_init(&(c->s), 2);
    cfe_uniform_sample_vec(&(c->s), c->order);

    // ECP_BN254_generator(&(c->client_pub_key));
    // cfe_uniform_sample(client_sec_key, c->order);
    // BIG_256_56_from_mpz(c->client_sec_key_big, client_sec_key);
    // ECP_BN254_mul(&(c->client_pub_key), c->client_sec_key_big);

    // cfe_mat_init(&(c->share), 2, 2);
    // mpz_clear(client_sec_key);
}

void cfe_dmcfe_client_free(cfe_dmcfe_client *c) {
    mpz_clear(c->order);
    cfe_mat_free(&(c->share));
    cfe_vec_free(&(c->s));
}

void cfe_dmcfe_set_share(cfe_dmcfe_client *c, ECP_BN254 *pub_keys, size_t num_clients) {
    cfe_mat add;
    cfe_mat_init(&add, 2, 2);
    ECP_BN254 shared_g1;
    char h1[MODBYTES_256_56 + 1];
    octet tmp_oct = {0, sizeof(h1), h1};
    char h2[randombytes_SEEDBYTES];
    octet tmp_hash = {0, sizeof(h2), h2};
    for (size_t k = 0; k < num_clients; k++) {
        if (k == c->idx) {
            continue;
        }

        ECP_BN254_copy(&shared_g1, &(pub_keys[k]));
        ECP_BN254_mul(&shared_g1, c->client_sec_key_big);
        ECP_BN254_toOctet(&tmp_oct, &shared_g1, true);

        mhashit(SHA256, -1, &tmp_oct, &tmp_hash);

        cfe_uniform_sample_mat_det(&add, c->order, ((unsigned char *) tmp_hash.val));

        if (k > c->idx) {
            cfe_mat_neg(&add, &add);
        }
        cfe_mat_add(&(c->share), &(c->share), &add);
        cfe_mat_mod(&(c->share), &(c->share), c->order);
    }

    cfe_mat_free(&add);
}

void cfe_dmcfe_encrypt(ECP_BN254 *cipher, cfe_dmcfe_client *c, mpz_t x, char *label, size_t label_len) {
    ECP_BN254 h;
    BIG_256_56 tmp_big;
    ECP_BN254_inf(cipher);
    cfe_string label_str = {label, label_len};
    cfe_string space_str = {(char *) " ", 1};
    cfe_string i_str, label_for_hash;
    for (int i = 0; i < 2; i++) {
        cfe_int_to_str(&i_str, i);
        cfe_strings_concat(&label_for_hash, &i_str, &space_str, &label_str, NULL);
        cfe_hash_G1(&h, &label_for_hash);
        BIG_256_56_from_mpz(tmp_big, c->s.vec[i]);
        ECP_BN254_mul(&h, tmp_big);
        ECP_BN254_add(cipher, &h);
        cfe_string_free(&label_for_hash);
        cfe_string_free(&i_str);
    }

    BIG_256_56_from_mpz(tmp_big, x);
    ECP_BN254_generator(&h);
    ECP_BN254_mul(&h, tmp_big);
    ECP_BN254_add(cipher, &h);
}

void cfe_dmcfe_fe_key_part_init(cfe_vec_G2 *key_share) {
    cfe_vec_G2_init(key_share, 2);
}

void cfe_dmcfe_derive_fe_key_part(cfe_vec_G2 *fe_key_part, cfe_dmcfe_client *c, cfe_vec *y) {
    // cfe_string str, str_i, for_hash;
    // cfe_string space_str = {(char *) " ", 1};
    // cfe_vec_to_string(&str, y);
    // ECP2_BN254 hash[2];
    // for (int i = 0; i < 2; i++) {
    //     cfe_int_to_str(&str_i, i);
    //     cfe_strings_concat(&for_hash, &str_i, &space_str, &str, NULL);
    //     cfe_hash_G2(&(hash[i]), &for_hash);
    //     cfe_string_free(&for_hash);
    //     cfe_string_free(&str_i);
    // }
    // cfe_string_free(&str);

    mpz_t tmp;
    mpz_init(tmp);
    BIG_256_56 tmp_big;
    ECP2_BN254 h;
    for (size_t k = 0; k < 2; k++) {
        ECP2_BN254_inf(&(fe_key_part->vec[k]));
        // for (size_t i = 0; i < 2; i++) {
        //     ECP2_BN254_copy(&h, &(hash[i]));
        //     cfe_mat_get(tmp, &(c->share), k, i);
        //     BIG_256_56_from_mpz(tmp_big, tmp);
        //     ECP2_BN254_mul(&(h), tmp_big);
        //     ECP2_BN254_add(&(fe_key_part->vec[k]), &h);
        // }

        mpz_mul(tmp, y->vec[c->idx], c->s.vec[k]);
        mpz_mod(tmp, tmp, c->order);
        //why generate
        BIG_256_56_from_mpz(tmp_big, tmp);
        ECP2_BN254_generator(&h);
        ECP2_BN254_mul(&h, tmp_big);
        ECP2_BN254_add(&(fe_key_part->vec[k]), &h);
    }
    mpz_clear(tmp);
}

cfe_error cfe_dmcfe_decrypt(mpz_t res, ECP_BN254 *ciphers, cfe_vec_G2 *key_shares,
                            char *label, size_t label_len, cfe_vec *y, mpz_t bound) {
    cfe_vec_G2 keys_sum;
    cfe_vec_G2_init(&keys_sum, 2);
    for (size_t i = 0; i < 2; i++) {
        ECP2_BN254_inf(&(keys_sum.vec[i]));
    }
    for (size_t k = 0; k < y->size; k++) {
        for (size_t i = 0; i < 2; i++) {
            ECP2_BN254_add(&(keys_sum.vec[i]), &(key_shares[k].vec[i]));
        }
    }
    ECP_BN254 ciphers_sum, cipher_i, gen1, h;
    ECP2_BN254 gen2;
    FP12_BN254 s, t, pair;
    ECP_BN254_generator(&gen1);
    ECP2_BN254_generator(&gen2);
    ECP_BN254_inf(&ciphers_sum);
    BIG_256_56 y_i;
    mpz_t y_i_mod, order;
    mpz_inits(y_i_mod, order, NULL);
    mpz_from_BIG_256_56(order, (int64_t *) CURVE_Order_BN254);

    for (size_t i = 0; i < y->size; i++) {
        ECP_BN254_copy(&cipher_i, &(ciphers[i]));
        mpz_mod(y_i_mod, y->vec[i], order);
        BIG_256_56_from_mpz(y_i, y_i_mod);
        ECP_BN254_mul(&cipher_i, y_i);
        ECP_BN254_add(&ciphers_sum, &cipher_i);
    }

    PAIR_BN254_ate(&s, &gen2, &ciphers_sum);
    PAIR_BN254_fexp(&s);
//
    cfe_string label_for_hash, str_i;
    cfe_string label_str = {label, label_len};
    cfe_string space_str = {(char *) " ", 1};
    FP12_BN254_one(&t);
    for (int i = 0; i < 2; i++) {
        cfe_int_to_str(&str_i, i);
        cfe_strings_concat(&label_for_hash, &str_i, &space_str, &label_str, NULL);
        cfe_hash_G1(&h, &label_for_hash);
        PAIR_BN254_ate(&pair, &(keys_sum.vec[i]), &h);
        PAIR_BN254_fexp(&pair);
        FP12_BN254_mul(&t, &pair);
        cfe_string_free(&label_for_hash);
        cfe_string_free(&str_i);
    }
    FP12_BN254_inv(&t, &t);
    FP12_BN254_mul(&s, &t);

    PAIR_BN254_ate(&pair, &gen2, &gen1);
    PAIR_BN254_fexp(&pair);

    mpz_t res_bound;
    mpz_init(res_bound);
    mpz_pow_ui(res_bound, bound, 2);
    mpz_mul_ui(res_bound, res_bound, y->size);

    cfe_error err;
    err = cfe_baby_giant_FP12_BN256_with_neg(res, &s, &pair, res_bound);

    mpz_clears(res_bound, y_i_mod, order, NULL);
    cfe_vec_G2_free(&keys_sum);

    return err;
}
