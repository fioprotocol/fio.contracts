/** FioName Token header file
 *  Description: FioName smart contract allows issuance of unique domains and names for easy public address resolution
 *  @author Adam Androulidakis, Ciju John, Casey Gardiner, Ed Rotthoff
 *  @file fio.address.hpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>

#include <string>

using std::string;

namespace fioio {

    using namespace eosio;
    using eosio::time_point;

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


    //the design pattern here is to perform the following for any indexes that
    //are read limited in FIO IE- there are more than 1500 or so rows with the same value
    //of secondary index.
    //the following pattern may only be applied when the table in question
    //has a primary index id (a one up for each row in the table).
    //we will define a FIO read block size on chain (we will set it initially to be 500)
    //this will be tunable for the protocol in state (IE we can change this read block size when necessary)
    //we will make a lookup table with columns
    //          id -- id of row
    //        index name -- the string rep of the index value (which will be "original index"+"fioblocknumber"
    //        index name hash
    //        table name -- state table to use
    //        table name hash
    //        read block -- the read block for this entry
    //        read block hash -- table name +index name + read block hashed
    //        tableid -- the primary index of the table in table name for the desired row.

    //
    //  indexes are
    //    primary
    //       id
    //
    //    secondary
    //       index name hash
    //       table name hash
    //       read block hash
    //
    //
    //   add a new action for each table block insert nft(with necessary parameters)
    //     this checks the
    //
    //
    // once we have this lookup table populated, getters may now use paging as follows
    //   read the fio read block size from state,
    //   compute the start block of the offset for the table roundup(offset/read block size)
    //   compute the end block of the limit specified roundup(limit/read block offset)
    // the getter can enforce the "sensible"use of limit and offset, throw an error when it is not sensible
    // (IE the getter must return less than 1500 records)
    //now to integrate the lookup table completely we must add logic whenever a row is inserted into the
    // table in question (to add a record to the lookup table by calling and whenever a record is removed from the table in question.


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


    struct [[eosio::action]] nftburninfo {
      uint64_t id;
      uint128_t fio_address_hash;
      uint128_t primary_key() const { return id; }
      uint128_t by_address() const { return fio_address_hash; }
      EOSLIB_SERIALIZE( nftburninfo, (id)(fio_address_hash))
    };


    typedef multi_index<"nftburnq"_n, nftburninfo,
            indexed_by<"byaddress"_n, const_mem_fun < nftburninfo, uint128_t, &nftburninfo::by_address>>
    >
    nftburnq_table;


}
