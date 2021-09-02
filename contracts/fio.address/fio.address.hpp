/** FioName Token header file
 *  Description: FioName smart contract allows issuance of unique domains and names for easy public address resolution
 *  @author Adam Androulidakis, Ciju John, Casey Gardiner, Ed Rotthoff
 *  @file fio.address.hpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/binary_extension.hpp>

#include <string>

using std::string;

namespace fioio {

    using namespace eosio;

    struct tokenpubaddr {
        string token_code;
        string chain_code;
        string public_address;
        EOSLIB_SERIALIZE( tokenpubaddr, (token_code)(chain_code)(public_address))
    };

    struct find_token {
        string token_code;
        find_token(string token_code) : token_code(token_code) {}
        bool operator () ( const tokenpubaddr& m ) const
        {
            return m.token_code == token_code;
        }
    };

    struct [[eosio::action]] fioname {

        uint64_t id = 0;
        string name = nullptr;
        uint128_t namehash = 0;
        string domain = nullptr;
        uint128_t domainhash = 0;
        uint64_t expiration;
        uint64_t owner_account;
        // Chain specific keys
        std::vector<tokenpubaddr> addresses;
        uint64_t bundleeligiblecountdown = 0;

        // primary_key is required to store structure in multi_index table
        uint64_t primary_key() const { return id; }
        uint128_t by_name() const { return namehash; }
        uint128_t by_domain() const { return domainhash; }
        uint64_t by_expiration() const { return expiration; }
        uint64_t by_owner() const { return owner_account; }

        EOSLIB_SERIALIZE(fioname, (id)(name)(namehash)(domain)(domainhash)(expiration)(owner_account)(addresses)(
                bundleeligiblecountdown)
        )
    };

    //Where fioname tokens are stored
    typedef multi_index<"fionames"_n, fioname,
            indexed_by<"bydomain"_n, const_mem_fun < fioname, uint128_t, &fioname::by_domain>>,
    indexed_by<"byexpiration"_n, const_mem_fun<fioname, uint64_t, &fioname::by_expiration>>,
    indexed_by<"byowner"_n, const_mem_fun<fioname, uint64_t, &fioname::by_owner>>,
    indexed_by<"byname"_n, const_mem_fun<fioname, uint128_t, &fioname::by_name>>
    >
    fionames_table;

    struct [[eosio::action]] domain {
        uint64_t id;
        string name;
        uint128_t domainhash;
        uint64_t account;
        uint8_t is_public = 0;
        uint64_t expiration;


        uint64_t primary_key() const { return id; }
        uint64_t by_account() const { return account; }
        uint64_t by_expiration() const { return expiration; }
        uint128_t by_name() const { return domainhash; }

        EOSLIB_SERIALIZE(domain, (id)(name)(domainhash)(account)(is_public)(expiration)
        )
    };

    typedef multi_index<"domains"_n, domain,
            indexed_by<"byaccount"_n, const_mem_fun < domain, uint64_t, &domain::by_account>>,
    indexed_by<"byexpiration"_n, const_mem_fun<domain, uint64_t, &domain::by_expiration>>,
    indexed_by<"byname"_n, const_mem_fun<domain, uint128_t, &domain::by_name>>
    >
    domains_table;

    // Maps client wallet generated public keys to EOS user account names.
    struct [[eosio::action]] eosio_name {

        uint64_t account = 0;
        string clientkey = nullptr;
        uint128_t keyhash = 0;

        uint64_t primary_key() const { return account; }
        uint128_t by_keyhash() const { return keyhash; }

        EOSLIB_SERIALIZE(eosio_name, (account)(clientkey)(keyhash)
        )
    };

    typedef multi_index<"accountmap"_n, eosio_name,
            indexed_by<"bykey"_n, const_mem_fun < eosio_name, uint128_t, &eosio_name::by_keyhash>>
    >
    eosio_names_table;

    // Maps NFT information to FIO Address
    struct [[eosio::action]] nftinfo {
      uint64_t id;
      string fio_address; //fio_address
      string chain_code;
      uint64_t chain_code_hash;
      string token_id;
      uint128_t token_id_hash;
      string url;
      uint128_t fio_address_hash;
      string contract_address;
      uint128_t contract_address_hash;
      string hash;
      uint128_t hash_index;
      string metadata;
      eosio::binary_extension<uint64_t> property1;
      eosio::binary_extension<uint128_t> property2;

      uint64_t primary_key() const { return id; }
      uint128_t by_address() const { return fio_address_hash; }
      uint128_t by_contract_address() const { return contract_address_hash; }
      uint128_t by_hash() const { return hash_index; }
      uint64_t  by_chain() const { return chain_code_hash; }
      uint128_t by_tokenid() const { return token_id_hash; }
      EOSLIB_SERIALIZE(nftinfo, (id)(fio_address)(chain_code)(chain_code_hash)(token_id)(token_id_hash)(url)(fio_address_hash)(contract_address)(contract_address_hash)
        (hash)(hash_index)(metadata))

    };

    typedef multi_index<"nfts"_n, nftinfo,
    indexed_by<"byaddress"_n, const_mem_fun<nftinfo, uint128_t, &nftinfo::by_address>>,
    indexed_by<"bycontract"_n, const_mem_fun<nftinfo, uint128_t, &nftinfo::by_contract_address>>,
    indexed_by<"byhash"_n, const_mem_fun<nftinfo, uint128_t, &nftinfo::by_hash>>,
    indexed_by<"bychain"_n, const_mem_fun<nftinfo, uint64_t, &nftinfo::by_chain>>,
    indexed_by<"bytokenid"_n, const_mem_fun<nftinfo, uint128_t, &nftinfo::by_tokenid>>

    >
    nfts_table;

    struct nftparam {
      string chain_code;
      string contract_address;
      string token_id;
      string url;
      string hash;
      string metadata;

      EOSLIB_SERIALIZE( nftparam, (chain_code)(contract_address)(token_id)(url)(hash)(metadata))
    };

    struct remnftparam {
      string chain_code;
      string contract_address;
      string token_id;

      EOSLIB_SERIALIZE( remnftparam, (chain_code)(contract_address)(token_id))
    };

}
