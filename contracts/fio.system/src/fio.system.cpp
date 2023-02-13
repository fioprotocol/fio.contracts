/** fio system contract file
 *  Description: this contract controls many of the core functions of the fio protocol.
 *  @author Adam Androulidakis, Casey Gardiner, Ed Rotthoff
 *  @file fio.system.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 *
 *  Changes:
 */
#include <fio.system/fio.system.hpp>
#include <eosiolib/dispatcher.hpp>
#include <eosiolib/crypto.h>
#include "producer_pay.cpp"
#include "delegate_bandwidth.cpp"
#include "voting.cpp"
#include <fio.address/fio.address.hpp>
#include <fio.token/fio.token.hpp>
#include <fio.fee/fio.fee.hpp>

namespace eosiosystem {

    system_contract::system_contract(name s, name code, datastream<const char *> ds)
            : native(s, code, ds),
              _voters(_self, _self.value),
              _producers(_self, _self.value),
              _topprods(_self, _self.value),
              _global(_self, _self.value),
              _global2(_self, _self.value),
              _global3(_self, _self.value),
              _lockedtokens(_self,_self.value),
              _generallockedtokens(_self, _self.value),
              _fionames(AddressContract, AddressContract.value),
              _domains(AddressContract, AddressContract.value),
              _accountmap(AddressContract, AddressContract.value),
              _fiofees(FeeContract, FeeContract.value){
        _gstate = _global.exists() ? _global.get() : get_default_parameters();
        _gstate2 = _global2.exists() ? _global2.get() : eosio_global_state2{};
        _gstate3 = _global3.exists() ? _global3.get() : eosio_global_state3{};
    }

    eosiosystem::eosio_global_state eosiosystem::system_contract::get_default_parameters() {
        eosio_global_state dp;
        get_blockchain_parameters(dp);
        return dp;
    }

    time_point eosiosystem::system_contract::current_time_point() {
        const static time_point ct{microseconds{static_cast<int64_t>( current_time())}};
        return ct;
    }

    time_point_sec eosiosystem::system_contract::current_time_point_sec() {
        const static time_point_sec cts{current_time_point()};
        return cts;
    }

    block_timestamp eosiosystem::system_contract::current_block_time() {
        const static block_timestamp cbt{current_time_point()};
        return cbt;
    }

    eosiosystem::system_contract::~system_contract() {
        _global.set(_gstate, _self);
        _global2.set(_gstate2, _self);
        _global3.set(_gstate3, _self);
    }

    void eosiosystem::system_contract::setparams(const eosio::blockchain_parameters &params) {
        require_auth(_self);
        (eosio::blockchain_parameters & )(_gstate) = params;
        check(3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3");
        set_blockchain_parameters(params);
    }

    void eosiosystem::system_contract::setpriv(const name &account, const uint8_t &ispriv) {
        require_auth(_self);
        set_privileged(account.value, ispriv);

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
          "Transaction is too large", ErrorTransactionTooLarge);

    }

    void eosiosystem::system_contract::rmvproducer(const name &producer) {
        require_auth(_self);
        auto prod = _producers.find(producer.value);
        check(prod != _producers.end(), "producer not found");
        _producers.modify(prod, same_payer, [&](auto &p) {
            p.deactivate();
        });

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
          "Transaction is too large", ErrorTransactionTooLarge);

    }

    void eosiosystem::system_contract::updtrevision(const uint8_t &revision) {
        require_auth(_self);
        check(_gstate2.revision < 255, "can not increment revision"); // prevent wrap around
        check(revision == _gstate2.revision + 1, "can only increment revision by one");
        check(revision <= 1, // set upper bound to greatest revision supported in the code
              "specified revision is not yet supported by the code");
        _gstate2.revision = revision;
    }

    //FIP-38 begin









    void eosiosystem::system_contract::newfioacc(const string &fio_public_key, const authority &owner, const authority &active, const int64_t &max_fee,
                                                 const name &actor,
                                                 const string &tpid) {


        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                       "TPID must be empty or valid FIO address",
                       ErrorPubKeyValid);
        fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                       ErrorMaxFeeInvalid);

        if (fio_public_key.length() > 0) {
            fio_400_assert(isPubKeyValid(fio_public_key), "fio_public_key", fio_public_key,
                           "Invalid FIO Public Key",
                           ErrorPubKeyValid);
        }

        string owner_account;
        key_to_account(fio_public_key, owner_account);
        name owner_account_name = name(owner_account.c_str());

        eosio_assert(owner_account.length() == 12, "Length of account name should be 12");

        //account should NOT exist, and should NOT be in the FIO account map
        const bool accountExists = is_account(owner_account_name);
        auto other = _accountmap.find(owner_account_name.value);

        fio_400_assert(!accountExists, "fio_public_key", fio_public_key,
                       "Invalid public key used, Account already exists on FIO chain",
                       ErrorPubAddressExist);

        if (other == _accountmap.end()) { //the name is not in the table. go forth and create the account

            const auto owner_pubkey = abieos::string_to_public_key(fio_public_key);

            eosiosystem::key_weight pubkey_weight = {
                    .key = owner_pubkey,
                    .weight = 1,
            };

            authority owner_auth = owner;
            if ((owner.accounts.size() == 0)&&(owner.keys.size() == 0)) {
                owner_auth = authority{1, {pubkey_weight}, {}, {}};
            }

            authority active_auth = active;
            if ((active.accounts.size() == 0)&&(active.keys.size()==0)) {
                active_auth = authority{1, {pubkey_weight}, {}, {}};
            }

            action(permission_level{SYSTEMACCOUNT, "active"_n},
                   SYSTEMACCOUNT, "newaccount"_n,
                   make_tuple(_self, owner_account_name, owner_auth,active_auth)
            ).send();

            action{
                    permission_level{_self, "active"_n},
                    AddressContract,
                    "bind2eosio"_n,
                    bind2eosio{
                            .accountName = owner_account_name,
                            .public_key = fio_public_key,
                            .existing = accountExists
                    }
            }.send();

        } else {
            fio_400_assert(accountExists, "fio_public_key", fio_public_key,
                           "Account does not exist on FIO chain but is bound in accountmap",
                           ErrorPubAddressExist);
        }


        const uint128_t endpoint_hash = string_to_uint128_hash(NEW_FIO_CHAIN_ACCOUNT_ENDPOINT);

        auto fees_by_endpoint = _fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);
        fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", NEW_FIO_CHAIN_ACCOUNT_ENDPOINT,
                       "FIO fee not found for endpoint", ErrorNoEndpoint);

        const uint64_t reg_amount = fee_iter->suf_amount;
        const uint64_t fee_type = fee_iter->type;

        fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                       "unexpected fee type for endpoint new_fio_chain_account, expected 0",
                       ErrorNoEndpoint);

        fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                       "Fee exceeds supplied maximum.",
                       ErrorMaxFeeExceeded);

        fio_fees(actor, asset(reg_amount, FIOSYMBOL), NEW_FIO_CHAIN_ACCOUNT_ENDPOINT);
        processbucketrewards(tpid, reg_amount, get_self(), actor);

        if (NEWFIOCHAINACCOUNTRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, NEWFIOCHAINACCOUNTRAM)
            ).send();
        }
        const string response_string = string("{\"status\": \"OK\",\"account\":\"") +
                                       owner_account + string("\",\"fee_collected\":") +
                                       to_string(reg_amount) + string("}");


        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                       "Transaction is too large", ErrorTransactionTooLarge);

        send_response(response_string.c_str());
    }
    //FIP-38 end


    /**
     *  Called after a new account is created. This code enforces resource-limits rules
     *  for new accounts as well as new account naming conventions.
     *
     *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
     *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
     *  who can create accounts with the creator's name as a suffix.
     *
     */
    void eosiosystem::native::newaccount(const name &creator,
                                         const name &newact,
                                          ignore <authority> owner,
                                          ignore <authority> active) {


        require_auth(creator);

        check((creator == SYSTEMACCOUNT || creator == TokenContract ||
                 creator == AddressContract), "new account is not permitted");

        if (creator != _self) {
            uint64_t tmp = newact.value >> 4;
            bool has_dot = false;

            for (uint32_t i = 0; i < 12; ++i) {
                has_dot |= !(tmp & 0x1f);
                tmp >>= 5;
            }
            if (has_dot) { // or is less than 12 characters
                auto suffix = newact.suffix();
                if (suffix != newact) {
                    check(creator == suffix, "only suffix may create this account");
                }
            }
        }


       user_resources_table userres(_self, newact.value);

        userres.emplace(newact, [&](auto &res) {
            res.owner = newact;
            res.net_weight = asset(0, FIOSYMBOL);
            res.cpu_weight = asset(0, FIOSYMBOL);
        });

        set_resource_limits(newact.value, INITIALACCOUNTRAM, -1, -1);

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
          "Transaction is too large", ErrorTransactionTooLarge);

    }

    void eosiosystem::native::addaction(const name &action, const string &contract, const name &actor) {

    }
    void eosiosystem::native::remaction(const name &action, const name &actor) {

    }


    void eosiosystem::native::setabi(const name &acnt, const std::vector<char> &abi) {

        require_auth(acnt);
        check((acnt == SYSTEMACCOUNT ||
                      acnt == MSIGACCOUNT ||
                      acnt == WRAPACCOUNT ||
                      acnt == ASSERTACCOUNT ||
                      acnt == REQOBTACCOUNT ||
                      acnt == FeeContract ||
                      acnt == AddressContract ||
                      acnt == TPIDContract ||
                      acnt == TokenContract ||
                      acnt == TREASURYACCOUNT ||
                      acnt == STAKINGACCOUNT ||
                      acnt == FIOSYSTEMACCOUNT ||
                      acnt == EscrowContract ||
                      acnt == FIOORACLEContract ||
                      acnt == FIOACCOUNT),"set abi not permitted." );


        eosio::multi_index<"abihash"_n, abi_hash> table(_self, _self.value);
        auto itr = table.find(acnt.value);
        if (itr == table.end()) {
            table.emplace(acnt, [&](auto &row) {
                row.owner = acnt;
                sha256(const_cast<char *>(abi.data()), abi.size(), &row.hash);
            });
        } else {
            table.modify(itr, same_payer, [&](auto &row) {
                sha256(const_cast<char *>(abi.data()), abi.size(), &row.hash);
            });
        }
    }

    void eosiosystem::system_contract::init(const unsigned_int &version, const symbol &core) {
        require_auth(_self);
        check(version.value == 0, "unsupported version for init action");
    }

    void eosiosystem::system_contract::setnolimits(const name &account) {
        eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(FIOSYSTEMACCOUNT)),
                     "missing required authority of fio.system or eosio");
        check(is_account(account),"account must pre exist");
        set_resource_limits(account.value, -1, -1, -1);
    }


    //use this action to initialize the locked token holders table for the FIO protocol.
    void eosiosystem::system_contract::addlocked(const name &owner, const int64_t &amount,
            const int16_t &locktype) {
        require_auth(_self);

        check(is_account(owner),"account must pre exist");
        check(amount > 0,"cannot add locked token amount less or equal 0.");
        check(locktype == 1 || locktype == 2 || locktype == 3 || locktype == 4,"lock type must be 1,2,3,4");

        _lockedtokens.emplace(owner, [&](struct locked_token_holder_info &a) {
                a.owner = owner;
                a.total_grant_amount = amount;
                a.unlocked_period_count = 0;
                a.grant_type = locktype;
                a.inhibit_unlocking = 1;
                a.remaining_locked_amount = amount;
                a.timestamp = now();
            });
        //return status added for staking, to permit unit testing using typescript sdk.
        const string response_string = string("{\"status\": \"OK\"}");
        send_response(response_string.c_str());
    }

    void eosiosystem::system_contract::addgenlocked(const name &owner, const vector<lockperiodv2> &periods, const bool &canvote,
            const int64_t &amount) {

        eosio_assert((has_auth(TokenContract) || has_auth(StakingContract)),
                     "missing required authority of fio.token or fio.staking");

        check(is_account(owner),"account must pre exist");
        check(amount > 0,"cannot add locked token amount less or equal 0.");

        _generallockedtokens.emplace(owner, [&](struct locked_tokens_info_v2 &a) {
            a.id = _generallockedtokens.available_primary_key();
            a.owner_account = owner;
            a.lock_amount = amount;
            a.payouts_performed = 0;
            a.can_vote = canvote?1:0;
            a.periods = periods;
            a.remaining_lock_amount = amount;
            a.timestamp = now();
        });
    }

    void eosiosystem::system_contract::modgenlocked(const name &owner, const vector<lockperiodv2> &periods,
                                                    const int64_t &amount, const int64_t &rem_lock_amount,
                                                    const uint32_t &payouts) {

        eosio_assert( has_auth(StakingContract) || has_auth(TokenContract),
                     "missing required authority of fio.staking or fio.token");

        check(is_account(owner),"account must pre exist");
        check(amount > 0,"cannot add locked token amount less or equal 0.");
        check(rem_lock_amount > 0,"cannot add remaining locked token amount less or equal 0.");
        check(payouts >= 0,"cannot add payouts less than 0.");

        uint64_t tota = 0;

        for(int i=0;i<periods.size();i++){
            fio_400_assert(periods[i].amount > 0, "unlock_periods", "Invalid unlock periods",
                           "Invalid amount value in unlock periods", ErrorInvalidUnlockPeriods);
            fio_400_assert(periods[i].duration > 0, "unlock_periods", "Invalid unlock periods",
                           "Invalid duration value in unlock periods", ErrorInvalidUnlockPeriods);
            tota += periods[i].amount;
            if (i>0){
                fio_400_assert(periods[i].duration > periods[i-1].duration, "unlock_periods", "Invalid unlock periods",
                               "Invalid duration value in unlock periods, must be sorted", ErrorInvalidUnlockPeriods);
            }
        }

        fio_400_assert(tota == amount, "unlock_periods", "Invalid unlock periods",
                       "Invalid total amount for unlock periods", ErrorInvalidUnlockPeriods);

        auto locks_by_owner = _generallockedtokens.get_index<"byowner"_n>();
        auto lockiter = locks_by_owner.find(owner.value);
        check(lockiter != locks_by_owner.end(),"error looking up lock owner.");
        //call the system contract and update the record.
        locks_by_owner.modify(lockiter, get_self(), [&](auto &av) {
            av.remaining_lock_amount = rem_lock_amount;
            av.lock_amount = amount;
            av.payouts_performed = payouts;
            av.periods = periods;

        });
    }


    //this action will check if all periods are in the past and clear the general locks if all of them are in the past.
    void eosiosystem::system_contract::clrgenlocked(const name &owner) {

        eosio_assert((has_auth(AddressContract) || has_auth(TokenContract) || has_auth(TREASURYACCOUNT) ||
                      has_auth(STAKINGACCOUNT) || has_auth(REQOBTACCOUNT) || has_auth(SYSTEMACCOUNT) ||
                      has_auth(FIOORACLEContract) || has_auth(FeeContract) || has_auth(EscrowContract)),
                     "missing required authority of fio.address, fio.token, fio.fee, fio.treasury, fio.oracle, fio.escrow, fio.staking, or fio.reqobt");
        check(is_account(owner), "account must pre exist");
        auto locks_by_owner = _generallockedtokens.get_index<"byowner"_n>();
        auto lockiter = locks_by_owner.find(owner.value);
        if (lockiter != locks_by_owner.end()) {
            uint32_t present_time = now();
            //never clear another accounts stuff.
            if ((lockiter->owner_account == owner) &&
                (((lockiter->periods[lockiter->periods.size() - 1].duration + lockiter->timestamp) < present_time) ||
                 lockiter->periods.size() == 0)) {
                locks_by_owner.erase(lockiter);
            }
        }
    }
} /// fio.system


EOSIO_DISPATCH( eosiosystem::system_contract,
// native.hpp (newaccount definition is actually in fio.system.cpp)
(newaccount)(addaction)(remaction)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setabi)
// fio.system.cpp
(init)(setnolimits)(addlocked)(addgenlocked)(modgenlocked)(clrgenlocked)(setparams)(setpriv)
        (rmvproducer)(updtrevision)(newfioacc)
// delegate_bandwidth.cpp
        (updatepower)
// voting.cpp
        (regproducer)(regiproducer)(unregprod)(voteproducer)(voteproxy)(inhibitunlck)
        (updlocked)(unlocktokens)(setautoproxy)(crautoproxy)(burnaction)(incram)
        (unregproxy)(regiproxy)(regproxy)
// producer_pay.cpp
        (onblock)
        (resetclaim)
(updlbpclaim)
)
