/** FIO Request OBT header file
 *  Description: The FIO request other blockchain transaction (or obt) contract contains the
 *  tables that hold the requests for funds that have been made along with the status of any
 *  response to each request for funds.
 *  @author Adam Androulidakis, Casey Gardiner, Ciju John, Ed Rotthoff
 *  @file fio.request.obt.hpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */


#pragma once

#include <eosiolib/eosio.hpp>
#include <string>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>

using std::string;

namespace fioio {

    using namespace eosio;

    // The status of a transaction progresses from requested, to either rejected or sent to blockchain (if funds
    // are sent in response to the request.
    enum class trxstatus {
        requested = 0,
        rejected = 1,
        sent_to_blockchain = 2,
        cancelled = 3,
        obt_action = 4,
        other = 5 //Future Use
    };

    struct ledgerItems {
        std::vector <uint64_t> pending_action_ids;
        std::vector <uint64_t> sent_action_ids;
        std::vector <uint64_t> cancelled_action_ids;
        std::vector <uint64_t> obt_action_ids;

        std::vector <uint64_t> other_ids; //Future Use
    };

    // The request context table holds the requests for funds that have been requested, it provides
    // searching by id, payer and payee.
    // @abi table fioreqctxts i64
    struct [[eosio::action]] fioreqctxt {
        uint64_t fio_request_id;
        uint128_t payer_fio_address;
        uint128_t payee_fio_address;
        string payer_fio_address_hex_str;
        string payee_fio_address_hex_str;
        uint128_t payer_fio_address_with_time;
        uint128_t payee_fio_address_with_time;
        string content;  //this content is a encrypted blob containing the details of the request.
        uint64_t time_stamp;
        string payer_fio_addr;
        string payee_fio_addr;
        string payer_key = nullptr;
        string payee_key = nullptr;

        uint64_t primary_key() const { return fio_request_id; }
        uint128_t by_receiver() const { return payer_fio_address; }
        uint128_t by_originator() const { return payee_fio_address; }
        uint128_t by_payerwtime() const { return payer_fio_address_with_time;}
        uint128_t by_payeewtime() const { return payee_fio_address_with_time;}

        EOSLIB_SERIALIZE(fioreqctxt,
        (fio_request_id)(payer_fio_address)(payee_fio_address)(payer_fio_address_hex_str)(payee_fio_address_hex_str)
                (payer_fio_address_with_time)(payee_fio_address_with_time)
        (content)(time_stamp)(payer_fio_addr)(payee_fio_addr)(payer_key)(payee_key)
        )
    };


    typedef multi_index<"fioreqctxts"_n, fioreqctxt,
            indexed_by<"byreceiver"_n, const_mem_fun < fioreqctxt, uint128_t, &fioreqctxt::by_receiver>>,
    indexed_by<"byoriginator"_n, const_mem_fun<fioreqctxt, uint128_t, &fioreqctxt::by_originator>>,
    indexed_by<"bypayerwtime"_n, const_mem_fun<fioreqctxt, uint128_t, &fioreqctxt::by_payerwtime>>,
    indexed_by<"bypayeewtime"_n, const_mem_fun<fioreqctxt, uint128_t, &fioreqctxt::by_payeewtime>
    >>
    fiorequest_contexts_table;

    //this struct holds records relating to the items sent via record obt, we call them recordobt_info items.
    struct [[eosio::action]] recordobt_info {
        uint64_t id;
        uint128_t payer_fio_address;
        uint128_t payee_fio_address;
        string payer_fio_address_hex_str;
        string payee_fio_address_hex_str;
        uint128_t payer_fio_address_with_time;
        uint128_t payee_fio_address_with_time;
        string content;  //this content is a encrypted blob containing the details of the request.
        uint64_t time_stamp;
        string payer_fio_addr;
        string payee_fio_addr;
        string payer_key = nullptr;
        string payee_key = nullptr;

        uint64_t primary_key() const { return id; }
        uint128_t by_payee() const { return payee_fio_address; }
        uint128_t by_payer() const { return payer_fio_address; }
        uint128_t by_payeewtime() const { return payee_fio_address_with_time; }
        uint128_t by_payerwtime() const { return payer_fio_address_with_time; }


        EOSLIB_SERIALIZE(recordobt_info,
        (id)(payer_fio_address)(payee_fio_address)(payer_fio_address_hex_str)(payee_fio_address_hex_str)
        (payer_fio_address_with_time)(payee_fio_address_with_time)
        (content)(time_stamp)(payer_fio_addr)(payee_fio_addr)(payer_key)(payee_key)
        )
    };

    typedef multi_index<"recordobts"_n, recordobt_info,
            indexed_by<"bypayee"_n, const_mem_fun < recordobt_info, uint128_t, &recordobt_info::by_payee>>,
    indexed_by<"bypayer"_n, const_mem_fun<recordobt_info, uint128_t, &recordobt_info::by_payer>>,
    indexed_by<"bypayerwtime"_n, const_mem_fun<recordobt_info, uint128_t, &recordobt_info::by_payerwtime>>,
    indexed_by<"bypayeewtime"_n, const_mem_fun<recordobt_info, uint128_t, &recordobt_info::by_payeewtime>
    >>
    recordobt_table;

    // The FIO request status table references FIO requests that have had funds sent, or have been rejected.
    // the table provides a means to find the status of a request.
    // @abi table fioreqstss i64
    struct [[eosio::action]] fioreqsts {
        uint64_t id;
        uint64_t fio_request_id;
        uint64_t status;
        string metadata;   //this contains a json string with meta data regarding the response to a request.
        uint64_t time_stamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_fioreqid() const { return fio_request_id; }

        EOSLIB_SERIALIZE(fioreqsts, (id)(fio_request_id)(status)(metadata)(time_stamp)
        )
    };

    typedef multi_index<"fioreqstss"_n, fioreqsts,
            indexed_by<"byfioreqid"_n, const_mem_fun < fioreqsts, uint64_t, &fioreqsts::by_fioreqid> >
    >
    fiorequest_status_table;

    // The request context table holds the requests for funds that have been requested, it provides
    // searching by id, payer and payee.
    // @abi table fioreqctxts i64
    struct [[eosio::action]] fiotrxt {
        uint64_t id;
        uint64_t fio_request_id = 0;
        uint128_t payer_fio_addr_hex;
        uint128_t payee_fio_addr_hex;
        uint8_t fio_data_type; //trxstatus ids
        uint64_t init_time;
        string payer_fio_addr;
        string payee_fio_addr;
        string payer_key = nullptr;
        string payee_key = nullptr;
        uint128_t payer_key_hex;
        uint128_t payee_key_hex;

        string content = "";
        uint64_t update_time = 0;

        uint64_t primary_key() const { return id; }
        uint64_t by_requestid() const { return fio_request_id; }
        uint128_t by_receiver() const { return payer_fio_addr_hex; }
        uint128_t by_originator() const { return payee_fio_addr_hex; }
        uint128_t by_payerkey() const { return payer_key_hex; }
        uint128_t by_payeekey() const { return payee_key_hex; }

        EOSLIB_SERIALIZE(fiotrxt,
        (id)(fio_request_id)(payer_fio_addr_hex)(payee_fio_addr_hex)(fio_data_type)(init_time)
                (payer_fio_addr)(payee_fio_addr)(payer_key)(payee_key)(payer_key_hex)(payee_key_hex)
                (content)(update_time)
        )
    };

    typedef multi_index<"fiotrxts"_n, fiotrxt,
            indexed_by<"byrequestid"_n, const_mem_fun < fiotrxt, uint64_t, &fiotrxt::by_requestid>>,
    indexed_by<"byreceiver"_n, const_mem_fun<fiotrxt, uint128_t, &fiotrxt::by_receiver>>,
    indexed_by<"byoriginator"_n, const_mem_fun<fiotrxt, uint128_t, &fiotrxt::by_originator>>,
    indexed_by<"bypayerkey"_n, const_mem_fun<fiotrxt, uint128_t, &fiotrxt::by_payerkey>>,
    indexed_by<"bypayeekey"_n, const_mem_fun<fiotrxt, uint128_t, &fiotrxt::by_payeekey>
    >>
    fiotrxt_contexts_table;

    struct [[eosio::action]] reqledger {

        uint64_t account;
        ledgerItems transactions;

        uint64_t primary_key() const { return account; }

        EOSLIB_SERIALIZE(reqledger, (account)(transactions))
    };

    typedef multi_index<"reqledgers"_n, reqledger> reqledgers_table;

    struct [[eosio::action]] migrledger {

        uint64_t id;

        int beginobt = -1;
        int currentobt = 0;

        int beginrq = -1;
        int currentrq = 0;

        int beginsta = -1;
        int currentsta = 0;

        bool isFinished = false;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(migrledger, (id)(beginobt)(currentobt)(beginrq)(currentrq)(beginsta)(currentsta))
    };

    typedef multi_index<"migrledgers"_n, migrledger> migrledgers_table;
}
