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
    // @abi table fiotrxt_info i64
    struct [[eosio::action]] fiotrxt_info {
        uint64_t id;
        uint64_t fio_request_id = 0;
        uint128_t payer_fio_addr_hex;
        uint128_t payee_fio_addr_hex;
        uint8_t fio_data_type; //trxstatus ids
        uint64_t req_time;
        string payer_fio_addr;
        string payee_fio_addr;
        string payer_key = nullptr;
        string payee_key = nullptr;
        uint64_t payer_account;
        uint64_t payee_account;

        string req_content = "";
        string obt_metadata  = "";
        uint64_t obt_time = 0;

        uint64_t primary_key() const { return id; }
        uint64_t by_requestid() const { return fio_request_id; }
        uint128_t by_receiver() const { return payer_fio_addr_hex; }
        uint128_t by_originator() const { return payee_fio_addr_hex; }
        uint64_t by_payeracct() const { return payer_account; }
        uint64_t by_payeeacct() const { return payee_account; }
        uint64_t by_time() const { return req_time > obt_time ? req_time : obt_time; }

        //Searches by status using bit shifting
        uint64_t by_payerstat() const { return payer_account + static_cast<uint64_t>(fio_data_type); }
        uint64_t by_payeestat() const { return payee_account + static_cast<uint64_t>(fio_data_type); }
        uint64_t by_payerobt() const {
            return payer_account + (fio_data_type == 2 || fio_data_type == 4);
        }
        uint64_t by_payeeobt() const {
            return payee_account + (fio_data_type == 2 || fio_data_type == 4);
        }
        uint64_t by_payerreq() const {
            return payer_account + (fio_data_type <= 3);
        }
        uint64_t by_payeereq() const {
            return payee_account + (fio_data_type <= 3);
        }

        EOSLIB_SERIALIZE(fiotrxt_info,
        (id)(fio_request_id)(payer_fio_addr_hex)(payee_fio_addr_hex)(fio_data_type)(req_time)
                (payer_fio_addr)(payee_fio_addr)(payer_key)(payee_key)(payer_account)(payee_account)
                (req_content)(obt_metadata)(obt_time)
        )
    };

    typedef multi_index<"fiotrxtss"_n, fiotrxt_info,
            indexed_by<"byrequestid"_n, const_mem_fun < fiotrxt_info, uint64_t, &fiotrxt_info::by_requestid>>,
    indexed_by<"byreceiver"_n, const_mem_fun<fiotrxt_info, uint128_t, &fiotrxt_info::by_receiver>>,
    indexed_by<"byoriginator"_n, const_mem_fun<fiotrxt_info, uint128_t, &fiotrxt_info::by_originator>>,
    indexed_by<"bypayeracct"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payeracct>>,
    indexed_by<"bypayeeacct"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payeeacct>>,
    indexed_by<"bytime"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_time>>,
    indexed_by<"bypayerstat"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payerstat>>,
    indexed_by<"bypayeestat"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payeestat>>,
    indexed_by<"bypayerobt"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payerobt>>,
    indexed_by<"bypayeeobt"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payeeobt>>,
    indexed_by<"bypayerreq"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payerreq>>,
    indexed_by<"bypayeereq"_n, const_mem_fun<fiotrxt_info, uint64_t, &fiotrxt_info::by_payeereq>
    >>
    fiotrxts_contexts_table;

    struct [[eosio::action]] migrledger {

        uint64_t id;

        uint64_t beginobt = -1;
        uint64_t currentobt = 0;

        uint64_t beginrq = -1;
        uint64_t currentrq = 0;

        uint64_t currentsta = 0;

        uint8_t isFinished = 0;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(migrledger, (id)(beginobt)(currentobt)(beginrq)(currentrq)(currentsta)(isFinished))
    };

    typedef multi_index<"migrledgers"_n, migrledger> migrledgers_table;
}
