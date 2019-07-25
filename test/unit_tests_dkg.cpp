/*
  Copyright (C) 2018-2019 SKALE Labs

  This file is part of libBLS.

  libBLS is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  libBLS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libBLS.  If not, see <https://www.gnu.org/licenses/>.

  @file unit_tests_dkg.cpp
  @author Oleh Nikolaiev
  @date 2019
*/


#include <dkg/dkg.h>

#include <cstdlib>
#include <ctime>
#include <map>
#include <set>

#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>
#include <libff/algebra/exponentiation/exponentiation.hpp>

#include "bls/BLSPrivateKeyShare.h"
#include "bls/BLSSigShareSet.h"
#include "bls/BLSSignature.h"
#include "bls/BLSPublicKey.h"
#include "bls/BLSPrivateKey.h"
#include "bls/BLSutils.cpp"

#define BOOST_TEST_MODULE

#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(DkgAlgorithm)

    BOOST_AUTO_TEST_CASE(PolynomialValue) {
        signatures::Dkg obj = signatures::Dkg(3, 4);
        std::vector<libff::alt_bn128_Fr> polynomial = {libff::alt_bn128_Fr("1"),
                                                       libff::alt_bn128_Fr("0"), libff::alt_bn128_Fr("1")};

        libff::alt_bn128_Fr value = obj.PolynomialValue(polynomial, 5);

        BOOST_REQUIRE(value == libff::alt_bn128_Fr("26"));

        polynomial.clear();

        polynomial = {libff::alt_bn128_Fr("0"),
                      libff::alt_bn128_Fr("1"), libff::alt_bn128_Fr("0")};
        bool is_exception_caught = false;

        try {
            value = obj.PolynomialValue(polynomial, 5);
        } catch (std::runtime_error) {
            is_exception_caught = true;
        }

        BOOST_REQUIRE(is_exception_caught);
    }

    BOOST_AUTO_TEST_CASE(verification) {
        signatures::Dkg obj = signatures::Dkg(2, 2);

        auto polynomial_fst = obj.GeneratePolynomial();
        auto polynomial_snd = obj.GeneratePolynomial();

        std::vector<libff::alt_bn128_G2> verification_vector_fst = obj.VerificationVector(polynomial_fst);
        std::vector<libff::alt_bn128_G2> verification_vector_snd = obj.VerificationVector(polynomial_snd);

        libff::alt_bn128_Fr shared_by_fst_to_snd = obj.SecretKeyContribution(polynomial_snd)[1];
        libff::alt_bn128_Fr shared_by_snd_to_fst = obj.SecretKeyContribution(polynomial_fst)[0];

        BOOST_REQUIRE(obj.Verification(0, shared_by_snd_to_fst, verification_vector_fst));
        BOOST_REQUIRE(obj.Verification(1, shared_by_fst_to_snd, verification_vector_snd));


        // these lines show that only correctly generated by the algorithm values can be verified
        BOOST_REQUIRE(obj.Verification(0, shared_by_snd_to_fst + libff::alt_bn128_Fr::random_element(),
                                       verification_vector_fst) == false);
        BOOST_REQUIRE(obj.Verification(1, shared_by_fst_to_snd + libff::alt_bn128_Fr::random_element(),
                                       verification_vector_snd) == false);
    }

    std::default_random_engine rand_gen((unsigned int) time(0));

    std::shared_ptr<std::vector<size_t> > choose_rand_signers(size_t num_signed, size_t num_all) {
        std::vector<size_t> participants(num_all);
        for (size_t i = 0; i < num_all; ++i) participants.at(i) = i + 1;
        for (size_t i = 0; i < num_all - num_signed; ++i) {
            size_t ind4del = rand_gen() % participants.size();
            participants.erase(participants.begin() + ind4del);
        }
        return std::make_shared<std::vector<size_t>>(participants);
    }

    std::array<uint8_t, 32> GenerateRandHash() {
        std::array<uint8_t, 32> hash_byte_arr;
        for (size_t i = 0; i < 32; i++) {
            hash_byte_arr.at(i) = rand_gen() % 255;
        }
        return hash_byte_arr;
    }

    BOOST_AUTO_TEST_CASE(threshold_signs_equality) {

        for (size_t i = 0; i < 100; ++i) {
            size_t num_all = rand_gen() % 15 + 2;
            size_t num_signed = rand_gen() % (num_all - 1) + 1;

            std::shared_ptr<std::vector<std::shared_ptr<BLSPrivateKeyShare>>> Skeys = BLSPrivateKeyShare::generateSampleKeys(
                    num_signed, num_all)->first;

            std::shared_ptr< std::array<uint8_t, 32> > hash_ptr = std::make_shared< std::array<uint8_t, 32> >(GenerateRandHash());

            BLSSigShareSet sigSet(num_signed, num_all);
            BLSSigShareSet sigSet1(num_signed, num_all);

            std::string message;
            size_t msg_length = rand_gen() % 1000 + 2;
            for (size_t length = 0; length < msg_length; ++length) {
                message += char(rand_gen() % 128);
            }
            std::shared_ptr<std::string> msg_ptr = std::make_shared<std::string>(message);

            std::shared_ptr<std::vector<size_t> > participants = choose_rand_signers(num_signed, num_all);
            std::shared_ptr<std::vector<size_t> > participants1 = choose_rand_signers(num_signed, num_all);

            for (size_t i = 0; i < num_signed; ++i) {
                std::shared_ptr<BLSPrivateKeyShare> skey = Skeys->at(participants->at(i) - 1);
                std::shared_ptr<BLSSigShare> sigShare = skey->sign(hash_ptr, participants->at(i));
                sigSet.addSigShare(sigShare);

                std::shared_ptr<BLSPrivateKeyShare> skey1 = Skeys->at(participants1->at(i) - 1);
                std::shared_ptr<BLSSigShare> sigShare1 = skey1->sign(hash_ptr, participants1->at(i));
                sigSet1.addSigShare(sigShare1);
            }

            std::shared_ptr<BLSSignature> common_sig_ptr = sigSet.merge();
            std::shared_ptr<BLSSignature> common_sig_ptr1 = sigSet1.merge();

            BOOST_REQUIRE(*common_sig_ptr->getSig() == *common_sig_ptr1->getSig());
        }

    }

    BOOST_AUTO_TEST_CASE(private_keys_equality) {

        for (size_t i = 0; i < 100; ++i) {
            size_t num_all = rand_gen() % 15 + 2;
            size_t num_signed = rand_gen() % (num_all - 1) + 1;

            signatures::Dkg dkg_obj = signatures::Dkg(num_signed, num_all);
            const std::vector<libff::alt_bn128_Fr> pol = dkg_obj.GeneratePolynomial();
            std::vector<libff::alt_bn128_Fr> skeys = dkg_obj.SecretKeyContribution(pol);

            std::shared_ptr<std::vector<size_t> > participants = choose_rand_signers(num_signed, num_all);

            signatures::Bls bls_obj = signatures::Bls(num_signed, num_all);
            std::vector<libff::alt_bn128_Fr> lagrange_koefs = bls_obj.LagrangeCoeffs(*participants);
            libff::alt_bn128_Fr common_skey = libff::alt_bn128_Fr::zero();
            for (size_t i = 0; i < num_signed; ++i) {
                common_skey = common_skey + lagrange_koefs.at(i) * skeys.at(participants->at(i) - 1);
            }

            BOOST_REQUIRE(common_skey == pol.at(0));
        }

    }

    BOOST_AUTO_TEST_CASE(public_keys_equality) {

        for (size_t i = 0; i < 100; ++i) {
            size_t num_all = rand_gen() % 15 + 2;
            size_t num_signed = rand_gen() % (num_all - 1) + 1;

            signatures::Dkg dkg_obj = signatures::Dkg(num_signed, num_all);
            const std::vector<libff::alt_bn128_Fr> pol = dkg_obj.GeneratePolynomial();
            std::vector<libff::alt_bn128_Fr> skeys = dkg_obj.SecretKeyContribution(pol);
            libff::alt_bn128_G2 common_pkey = pol.at(0) * libff::alt_bn128_G2::one();

            std::shared_ptr<std::vector<size_t> > participants = choose_rand_signers(num_signed, num_all);

            signatures::Bls bls_obj = signatures::Bls(num_signed, num_all);
            std::vector<libff::alt_bn128_Fr> lagrange_koefs = bls_obj.LagrangeCoeffs(*participants);
            libff::alt_bn128_G2 common_pkey1 = libff::alt_bn128_G2::zero();
            for (size_t i = 0; i < num_signed; ++i) {
                common_pkey1 = common_pkey1 +
                               lagrange_koefs.at(i) * skeys.at(participants->at(i) - 1) * libff::alt_bn128_G2::one();
            }
            BOOST_REQUIRE(common_pkey == common_pkey1);
        }

    }


BOOST_AUTO_TEST_SUITE_END()
