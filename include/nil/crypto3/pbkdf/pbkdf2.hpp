//---------------------------------------------------------------------------//
// Copyright (c) 2018-2020 Mikhail Komarov <nemo@nil.foundation>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//---------------------------------------------------------------------------//

#ifndef CRYPTO3_PBKDF_PBKDF2_HPP
#define CRYPTO3_PBKDF_PBKDF2_HPP

#include <nil/crypto3/pbkdf/detail/pbkdf2/pbkdf2_functions.hpp>

#include <chrono>

namespace nil {
    namespace crypto3 {
        namespace pbkdf {
            /*!
             * @brief
             * @tparam MessageAuthenticationCode
             * @ingroup pbkdf
             */
            template<typename MessageAuthenticationCode>
            class pbkdf2 {
                typedef detail::pbkdf2_functions<MessageAuthenticationCode> policy_type;

            public:
                typedef typename policy_type::mac_type mac_type;

                constexpr static const std::size_t digest_bits = policy_type::digest_bits;
                typedef typename policy_type::digest_type digest_type;

                constexpr static const std::size_t salt_bits = policy_type::salt_bits;
                typedef typename policy_type::salt_type salt_type;

                size_t derive(digest_type &digest, const std::string &passphrase, const salt_type &salt,
                              size_t iterations, std::chrono::milliseconds msec) {
                    clear_mem(out, out_len);

                    if (out_len == 0) {
                        return 0;
                    }

                    try {
                        prf.set_key(cast_char_ptr_to_uint8(passphrase.data()), passphrase.size());
                    } catch (Invalid_Key_Length &) {
                        throw Exception("PBKDF2 with " + prf.name() + " cannot accept passphrases of length " +
                                        std::to_string(passphrase.size()));
                    }

                    const size_t prf_sz = prf.output_length();
                    secure_vector<uint8_t> U(prf_sz);

                    const std::size_t blocks_needed = policy_type::round_up(out_len, prf_sz) / prf_sz;

                    std::chrono::microseconds usec_per_block =
                        std::chrono::duration_cast<std::chrono::microseconds>(msec) / blocks_needed;

                    uint32_t counter = 1;
                    while (out_len) {
                        const size_t prf_output = std::min<size_t>(prf_sz, out_len);

                        prf.update(salt, salt_len);
                        prf.update_be(counter++);
                        prf.final(U.data());

                        xor_buf(out, U.data(), prf_output);

                        if (iterations == 0) {
                            /*
                            If no iterations set, run the first block to calibrate based
                            on how long hashing takes on whatever machine we're running on.
                            */

                            const auto start = std::chrono::high_resolution_clock::now();

                            iterations = 1;    // the first iteration we did above

                            while (true) {
                                prf.update(U);
                                prf.final(U.data());
                                xor_buf(out, U.data(), prf_output);
                                iterations++;

                                /*
                                Only break on relatively 'even' iterations. For one it
                                avoids confusion, and likely some broken implementations
                                break on getting completely randomly distributed values
                                */
                                if (iterations % 10000 == 0) {
                                    auto time_taken = std::chrono::high_resolution_clock::now() - start;
                                    auto usec_taken = std::chrono::duration_cast<std::chrono::microseconds>(time_taken);
                                    if (usec_taken > usec_per_block) {
                                        break;
                                    }
                                }
                            }
                        } else {
                            for (size_t i = 1; i != iterations; ++i) {
                                prf.update(U);
                                prf.final(U.data());
                                xor_buf(out, U.data(), prf_output);
                            }
                        }

                        out_len -= prf_output;
                        out += prf_output;
                    }

                    return iterations;
                }
            };
        }    // namespace pbkdf
    }        // namespace crypto3
}    // namespace nil

#endif
