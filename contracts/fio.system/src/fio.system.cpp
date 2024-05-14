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
              _fiofees(FeeContract, FeeContract.value),
              _auditglobal(_self,_self.value),
              _auditproxy(_self,_self.value),
              _auditproducer(_self,_self.value){
        _gstate = _global.exists() ? _global.get() : get_default_parameters();
        _gstate2 = _global2.exists() ? _global2.get() : eosio_global_state2{};
        _gstate3 = _global3.exists() ? _global3.get() : eosio_global_state3{};
        _audit_global_info = _auditglobal.exists() ? _auditglobal.get() : audit_global_info{};
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
        _auditglobal.set(_audit_global_info,_self);
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

    //todo need to write remove producer tests!!!!
    void eosiosystem::system_contract::rmvproducer(const name &producer) {
        require_auth(_self);
        auto prod = _producers.find(producer.value);
        check(prod->owner == producer,"producer not found");
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
                      acnt == FIOACCOUNT ||
                      acnt == PERMSACCOUNT),"set abi not permitted." );


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

        //BD-4082 begin
        auto locks_by_owner = _generallockedtokens.get_index<"byowner"_n>();
        auto lockiter = locks_by_owner.find(owner.value);
        bool haslocks = false;
        bool allexpired = false;
        // BD-4162 begin
        if (lockiter != locks_by_owner.end()) {
            haslocks = true;
            uint32_t present_time = now();
            //never clear another accounts stuff. check all locks in the past, remove if they are.
            if ((lockiter->owner_account == owner) &&
                (((lockiter->periods[lockiter->periods.size() - 1].duration + lockiter->timestamp) < present_time) ||
                 lockiter->periods.size() == 0)) {
                allexpired = true;
                locks_by_owner.erase(lockiter);
            }
        }

        //if previous locks and not all expired then error.
        check((haslocks && allexpired) || lockiter == locks_by_owner.end(),"cannot emplace locks when locks pre-exist.");
        //BD-4162 end
        //BD-4082 end

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


    //overwrite an existing lock, this should only occur when all locking periods are in the past from now.
    void eosiosystem::system_contract::ovrwrtgenlck(const name &owner, const vector<lockperiodv2> &periods,
                                                    const int64_t &amount,
                                                    const bool &canvote) {

        eosio_assert( has_auth(StakingContract) || has_auth(TokenContract),
                      "missing required authority of fio.staking or fio.token");

        check(is_account(owner),"account must pre exist");
        check(amount > 0,"cannot add locked token amount less or equal 0.");

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
            av.lock_amount = amount;
            av.payouts_performed = 0;
            av.can_vote = canvote?1:0;
            av.periods = periods;
            av.remaining_lock_amount = amount;
            av.timestamp = now();
        });
    }


    //this action will check if all periods are in the past and clear the general locks if all of them are in the past.
    void eosiosystem::system_contract::clrgenlocked(const name &owner) {

        eosio_assert((has_auth(AddressContract) || has_auth(TokenContract) || has_auth(TREASURYACCOUNT) ||
                      has_auth(STAKINGACCOUNT) || has_auth(REQOBTACCOUNT) || has_auth(SYSTEMACCOUNT) ||
                      has_auth(FIOORACLEContract) || has_auth(FeeContract) || has_auth(EscrowContract) ||
                      has_auth(PERMSACCOUNT)),
                     "missing required authority of fio.address, fio.token, fio.fee, fio.treasury, fio.oracle, fio.escrow, fio.staking, fio.perms or fio.reqobt");
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

    int system_contract::addtoproducervote(const name &voter,
                          const double &weight, const std::vector <name> &producers ){

        int opcount = 2;

        //check(producers.size() > 0,"cannot use empty producer list.");
        check(weight >= 0,"cannot use weight less than 0. account "+voter.to_string());

        auto auditprodbyaccount = _auditproducer.get_index<"byaccount"_n>();

        for(name prodnm : producers) {
            auto auditprodacct_iter = auditprodbyaccount.find(prodnm.value);
            if(auditprodacct_iter == auditprodbyaccount.end() ){
                uint64_t id = _auditproducer.available_primary_key();
                _auditproducer.emplace(_self, [&](auto &p) {
                    p.id = id;
                    p.account_name = prodnm;
                    p.voted_fio = weight;
                });


            } else {
                auditprodbyaccount.modify(auditprodacct_iter, _self, [&](struct audit_producer_info &a) {
                    a.voted_fio += weight;
                });
             }
            _audit_global_info.total_producer_vote_weight += weight;

        opcount ++;
        }


        return opcount;
    }

    //set a proxies relevant info into the auditproxy table when we see a proxy in the voters table.
    int system_contract::setproxyweight(const uint64_t &voterid,
                        const uint64_t &votable_balance,
                        const std::vector <name> &producers) {

        //always operation count 3, read the audit proxy table, check the audit proxy for existance in the
        //table, then mod or emplace a record.
        int opcount = 3;

        //get the voter from the audit proxy table.
        auto auditproxybyvoterid = _auditproxy.get_index<"byvotererid"_n>();
        auto auditproxy_iter = auditproxybyvoterid.find(voterid);

        //if the record does not exist then create it.
        if(auditproxy_iter == auditproxybyvoterid.end()) {
            uint64_t id = _auditproxy.available_primary_key();
            _auditproxy.emplace(_self, [&](auto &p) {
                p.id = id;
                p.voterid = voterid;
                p.votable_balance = votable_balance;
                p.proxied_vote_weight = 0;
                p.producers = producers;
            });
            //if the record does exist, just add the producers that have been voted for by this voter.
        } else {
            auditproxybyvoterid.modify(auditproxy_iter, _self, [&](struct audit_proxy_info &a) {
                a.votable_balance = votable_balance;
                a.producers = producers;
            });
        }


        return opcount;
    }

    // add a proxy participant to the proxy summary info for a proxy.
    // params
    // voterid of the proxy to which the voting account is proxying.
    // last vote weight of the voting account.
    int system_contract::addproxyweight(const uint64_t &voterid,const double &weight){

        //operation count is always 3, read the audit proxy table, check existence of the record in the table,
        //then emplace/update the record in the audit proxy table.
        int opcount = 3;

        //if any last vote weight is less than 0 fail.
        check(weight >= 0,"cannot use proxy weight less than 0 voter id "+to_string(voterid));

        auto auditproxybyvoterid = _auditproxy.get_index<"byvotererid"_n>();
        auto auditproxy_iter = auditproxybyvoterid.find(voterid);

        //if the audit proxy record does not exist then update the summary info for the proxy.
        //the voterid and the weight.
        if(auditproxy_iter == auditproxybyvoterid.end()) {
            uint64_t id = _auditproxy.available_primary_key();
            _auditproxy.emplace(_self, [&](auto &p) {
                p.id = id;
                p.voterid = voterid;
                p.proxied_vote_weight = weight;
                //no producers are set here!!,
                // producers get set later when we see a proxy that has voted.
                // just a note we check that producers is not empty when adding proxy to producer vote
                //during the write of the audit.
            });
        } else {
            //if the record exists then just add the weight to the proxied weight.
            auditproxybyvoterid.modify(auditproxy_iter, _self, [&](struct audit_proxy_info &a) {
                a.proxied_vote_weight += weight;
            });
        }


        return opcount;
    }


    //begin audit machine
    // call this action repeatedly and it will progress through the process of auditing the FIO vote.
    // phase 1, clear any previous audit data, phase 2 analyze the voters table contents iteratively on each call
    // build a summary of proxy voting weight and producer vote weight for non proxy voters, phase 3 roll up the proxy summary
    // into the producer vote weight, phase 4 write the audited vote out to the fio blockchain and update the vote with
    // the results of the audit.
    void eosiosystem::system_contract::auditvote(const name &actor,  const int64_t &max_fee){
        string response_string ="";


        eosio_assert((has_auth(actor)),
                     "missing required authority of actor account");

        //if the calling account is in the voters table this is an error.
        auto votersbyowner = _voters.get_index<"byowner"_n>();
        auto auditaccount_iter = votersbyowner.find(actor.value);
        check(auditaccount_iter == votersbyowner.end()," cannot call auditvote using an account that has voted, please use an account that has not voted.\n");

        //fees and ram bump
        const uint128_t endpoint_hash = string_to_uint128_hash(AUDIT_VOTE_FEE_ENDPOINT);

        auto fees_by_endpoint = _fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);
        fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", AUDIT_VOTE_FEE_ENDPOINT,
                       "FIO fee not found for endpoint", ErrorNoEndpoint);

        const uint64_t reg_amount = fee_iter->suf_amount;
        const uint64_t fee_type = fee_iter->type;

        fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                       "unexpected fee type for endpoint audit_vote, expected 0",
                       ErrorNoEndpoint);

        fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                       "Fee exceeds supplied maximum.",
                       ErrorMaxFeeExceeded);

        //get audit state.
       if( _audit_global_info.audit_reset){
           _audit_global_info.audit_reset = false;
           _audit_global_info.audit_phase = 1;

       }

       //init to 2 read the voters table for the calling account and verify not in voters table.
        int operationcount = 2;
        int recordcount = 0;

       switch(_audit_global_info.audit_phase){
           case 0: {
               _audit_global_info.audit_phase = 1;
               //always fall through to phase 1
           }
           case 1: {
               //phase 1 is assumed to execute in one block, if it cannot then phase 4 will also fail to transact,
               // so no number of operation checks are performed on phase 1, we just clear the data.
               //clear all audit_global_info values.
               _audit_global_info.total_voted_fio = 0;
               _audit_global_info.current_proxy_id = 0;
               _audit_global_info.current_voter_id = 0;
               _audit_global_info.total_producer_vote_weight = 0;
               recordcount = 4;


               //remove all audit producers records.
               for (auto idx = _auditproducer.begin(); idx != _auditproducer.end();) {
                       idx = _auditproducer.erase(idx);
                       recordcount++;
               }
               //remove all audit proxy records.
               for (auto idx2 = _auditproxy.begin(); idx2 != _auditproxy.end();) {
                   idx2 = _auditproxy.erase(idx2);
                   recordcount++;
               }

               _audit_global_info.audit_phase = 2;
               recordcount++;

               response_string = string("{\"status\": \"OK\",\"audit_phase\":\"") +
                                             to_string(_audit_global_info.audit_phase) + string("\",\"records_processed\": ") +
                                             to_string(recordcount) + string(",\"fee_collected\":") +
                                              to_string(reg_amount) + string("}");
               break;
           }
           case 2: {

               //first get the index at which to stop, this is the current available primary key value for the voters table.
               uint64_t stopidx = _voters.available_primary_key();

               //get the index at which to begin processing.
               uint64_t id = _audit_global_info.current_voter_id;
               while( id < stopidx) {
                   //get the current voter by id from voters table
                   auto voter = _voters.find(id);
                   //if the voter id is not found in the voters table we just go to the next one.
                   //gaps will appear in the voters table as a result of removing address and token contract entries
                   //in the voters table.
                   if (voter != _voters.end()) {
                       //if the voting account is token or address contracts then remvoe this record from the voters table
                       //as these accounts should not be voting.
                       if( (voter->owner == TokenContract) ||
                               (voter->owner == AddressContract) ){
                           //erase the record.
                           _voters.erase(voter);
                           //increment operation count +2 cuz we read the table and updated.
                           operationcount += 2;
                       } else {
                           //get the current votable balance of the account
                           uint64_t bal =  eosio::token::computeusablebalance(voter->owner,false, false);
                          // uint64_t bal = get_votable_balance(voter->owner);
                           //if the voter is not proxying to a proxy.
                           if (!voter->proxy) {
                               //check for the known data incoherency of the is auto proxy flag, correct this if its present.
                               if(voter->is_auto_proxy){
                                   //clear the is_auto_proxy
                                   _voters.modify(voter, _self, [&](struct voter_info &a) {
                                       a.is_auto_proxy = false;
                                   });
                                   // increase op count by 2 read this voter plus update.
                                   operationcount += 2;
                               }
                               //if not proxying add the vote power to the producer vote.
                               operationcount += addtoproducervote(voter->owner,
                                                                   bal, voter->producers);
                               //if its a proxy set the account votable balance to be the usable balance.
                               //if it is a proxy also record its vote, if its not longer a proxy then
                               //do not record a vote.
                               if(voter->is_proxy){
                                   operationcount += setproxyweight(voter->id, bal, voter->producers);
                               }else{ //we will update all proxied vote weights even for proxies that
                                    //have registered then unregistered.
                                   if (voter->proxied_vote_weight >0){
                                       //set the proxy in the table without producers,
                                       //this voter is not a proxy, so no producers list here.
                                       //this keeps the proxied vote weight being added into the producer totals
                                       // in phase 3 of the audit....tricky.
                                       std::vector <name> emptyprod;
                                       operationcount += setproxyweight(voter->id, bal, emptyprod);
                                   }
                               }
                               if(voter->producers.size() > 0) {
                                   _audit_global_info.total_voted_fio += bal;
                               }
                           }

                           //if its a proxy add the last vote weight to the audit proxy totals
                           else if (voter->proxy) {
                               //get the proxies voter id from the voters table.
                               auto votersbyaccount = _voters.get_index<"byowner"_n>();
                               auto proxy_iter = votersbyaccount.find(voter->proxy.value);
                               //increment by two read voter, read proxy.
                               operationcount += 2;
                               //if the proxy isnt in the voters table this is fine,
                               //if the proxy is in the voters table increment the audit proxy totals.
                               if(proxy_iter != votersbyaccount.end()) {
                                   //add this voters last vote weight to the audit proxy total.
                                   operationcount += addproxyweight(proxy_iter->id,
                                                                    voter->last_vote_weight);
                               }

                               if(proxy_iter->is_proxy){
                                  if(proxy_iter->producers.size() > 0) {
                                      _audit_global_info.total_voted_fio += bal;
                                  }
                               }
                           }else { //just a plain voter voting for no producers.
                              if(voter->producers.size() > 0) {
                                  _audit_global_info.total_voted_fio += bal;
                              }
                           }

                       }  //end else its not token or address contract account.
                   } //end if the voter id exists in voters
                   //go to the next record.
                   recordcount++;
                   id++;

                   if(operationcount >= 240) break;
               } //end loop
               //store the voter id at which to resume the audit
               _audit_global_info.current_voter_id = id;

               //if we have processed all ids then got to the next phase of the audit.
               if (id >= stopidx) {
                   _audit_global_info.audit_phase = 3;
               }

               // Return computed status string.
               response_string = string("{\"status\": \"OK\",\"audit_phase\":\"") +
                       to_string(_audit_global_info.audit_phase) + string("\",\"records_processed\": ") +
                                              to_string(recordcount) + string(",\"fee_collected\":") +
                                              to_string(reg_amount) + string("}");


               break;
           }
           case 3: {
               uint64_t stopidx = _auditproxy.available_primary_key();

               uint64_t id = _audit_global_info.current_proxy_id;

               while( id < stopidx) {
                   //get the auditproxy by id from voters table
                   auto audproxy = _auditproxy.find(id);
                   //if this error happens then vote with any account on chain to reset the audit!!!!
                   check(audproxy != _auditproxy.end(),"failed to find auditproxy id "+to_string(id)+"\n");

                   auto voter = _voters.find(audproxy->voterid);
                   //if the voter id is not found in the voters table we just go to the next one.
                   //if this error happens then vote with any account on chain to reset the audit!!!!
                   check (voter != _voters.end(),"failed to find proxy in voters table voterid "+to_string(audproxy->voterid)+"\n");

                   operationcount += 2;

                   if((audproxy->producers.size() > 0) && (audproxy->proxied_vote_weight > 0)) {
                       operationcount += addtoproducervote(voter->owner,audproxy->proxied_vote_weight, audproxy->producers);
                   }

                   id++;
                   recordcount++;
                   if(operationcount >= 120) break;
               } //end loop
               _audit_global_info.current_proxy_id = id;
               if (id >= stopidx) {
                   _audit_global_info.audit_phase = 4;
               }
               // Return computed status string.
               response_string = string("{\"status\": \"OK\",\"audit_phase\":\"") +
                                 to_string(_audit_global_info.audit_phase) + string("\",\"records_processed\": ") +
                                 to_string(recordcount) + string(",\"fee_collected\":") +
                                 to_string(reg_amount) + string("}");

               break;
           }
           case 4: {

               auto producersbyaccount = _producers.get_index<"byowner"_n>();

               for (auto idx = _auditproducer.begin(); idx != _auditproducer.end(); idx++) {
                   auto producer_iter = producersbyaccount.find(idx->account_name.value);
                   //if the producer is not found, do not complete
                   //if this error happens then vote with any account on chain to reset the audit!!!!
                   check (producer_iter != producersbyaccount.end(),"failed to find producer in voters table producers "+idx->account_name.to_string()+"\n");
                   //set producer vote weight.
                   producersbyaccount.modify(producer_iter, _self, [&](struct producer_info &p) {
                       p.total_votes = idx->voted_fio;
                   });
               }

               for (auto idx2 = _auditproxy.begin(); idx2 != _auditproxy.end(); idx2++) {
                   auto voter = _voters.find(idx2->voterid);
                   //if the voter id is not found this is an incoherency in the audit. do not complete.
                   //if this error happens then vote with any account on chain to reset the audit!!!!
                   check (voter != _voters.end(),"failed to find proxy in voters table voterid "+to_string(idx2->voterid)+"\n");
                   //set proxy vote weight. and last vote weight.
                   double last_vote_weight = (double)(idx2->votable_balance);
                   if(voter->is_proxy) {
                      last_vote_weight +=  idx2->proxied_vote_weight;
                   }
                   _voters.modify(voter, _self, [&](struct voter_info &a) {
                       a.last_vote_weight = last_vote_weight;
                       a.proxied_vote_weight = idx2->proxied_vote_weight;
                   });
               }

               _gstate.total_voted_fio = _audit_global_info.total_voted_fio;
               _gstate.total_producer_vote_weight =  _audit_global_info.total_producer_vote_weight;

               _audit_global_info.audit_phase = 1;
               response_string = string("{\"status\": \"OK\",\"audit_phase\":\"") +
                       to_string(_audit_global_info.audit_phase) + string("\",\"records_processed\": ") +
                                              to_string(0) + string(",\"fee_collected\":") +
                                              to_string(reg_amount) + string("}");


               break;
           }
           default :{
               //if anything is out of whack with teh phase values, then reset to phase 1.
               print("AUDITVOTE -- illegal phase value detected, resetting phase to phase 1.\n");
               _audit_global_info.audit_phase = 1;
               break;
           }
       }


        fio_fees(actor, asset(reg_amount, FIOSYMBOL), NEW_FIO_CHAIN_ACCOUNT_ENDPOINT);
        processbucketrewards("", reg_amount, get_self(), actor);

        if (AUDITVOTERAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, AUDITVOTERAM)
            ).send();
        }

        send_response(response_string.c_str());

    }

    void eosiosystem::system_contract::resetaudit(){
        eosio_assert( has_auth(TokenContract),
                     "missing required authority of fio.token account");
        _audit_global_info.audit_reset = true;
    }
    //end audit machine


} /// fio.system


EOSIO_DISPATCH( eosiosystem::system_contract,
// native.hpp (newaccount definition is actually in fio.system.cpp)
(newaccount)(addaction)(remaction)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setabi)
// fio.system.cpp
(init)(setnolimits)(addlocked)(addgenlocked)(modgenlocked)(ovrwrtgenlck)(clrgenlocked)(setparams)(setpriv)
        (rmvproducer)(updtrevision)(newfioacc)(auditvote)(resetaudit)
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
