/** Fio Staking implementation file
 *  Description:
 *  @author  Ed Rotthoff
 *  @modifedby
 *  @file fio.staking.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */
#include <eosiolib/eosio.hpp>
#include "fio.staking.hpp"
#include <fio.token/fio.token.hpp>
#include <fio.address/fio.address.hpp>
#include <fio.fee/fio.fee.hpp>
#include <fio.system/include/fio.system/fio.system.hpp>

#define ENABLESTAKINGREWARDSEPOCHSEC  1645552800//feb 22 2022 18:00:00 GMT  10-11AM MST

namespace fioio {

class [[eosio::contract("Staking")]]  Staking: public eosio::contract {

private:

        //these holds global staking state for fio
        global_staking_singleton         staking;
        global_staking_state             gstaking;
        account_staking_table            accountstaking;
        //access to the voters table for voting info.
        eosiosystem::voters_table        voters;
        //access to fionames for address info
        fionames_table                   fionames;
        //access to fio fees for computation of fees.
        fiofee_table                     fiofees;
        //access to general locks to adapt general locks on unstake
        eosiosystem::general_locks_table_v2 generallocks;

public:
        using contract::contract;

        Staking(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                staking(_self, _self.value),
                accountstaking(_self,_self.value),
                voters(SYSTEMACCOUNT,SYSTEMACCOUNT.value),
                fiofees(FeeContract, FeeContract.value),
                fionames(AddressContract, AddressContract.value),
                generallocks(SYSTEMACCOUNT,SYSTEMACCOUNT.value){
            gstaking = staking.exists() ? staking.get() : global_staking_state{};
        }

        ~Staking() {
            staking.set(gstaking, _self);
        }


    [[eosio::action]]
    void incgrewards(const int64_t &fioamountsufs) {
        eosio_assert((has_auth(AddressContract) || has_auth(TokenContract) || has_auth(TREASURYACCOUNT) ||
                      has_auth(STAKINGACCOUNT) || has_auth(REQOBTACCOUNT) || has_auth(SYSTEMACCOUNT) ||
                      has_auth(FeeContract) || has_auth(FIOORACLEContract) || has_auth(EscrowContract) ||
                             has_auth(PERMSACCOUNT) ),
                     "missing required authority of fio.address, fio.treasury, fio.fee, fio.token, fio.staking, fio.oracle, fio.escrow, eosio, fio.perms or fio.reqobt");

        const uint32_t present_time = now();
        gstaking.rewards_token_pool += fioamountsufs;
        gstaking.daily_staking_rewards += fioamountsufs;
        gstaking.combined_token_pool += fioamountsufs;
        if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
            gstaking.last_combined_token_pool = gstaking.combined_token_pool;
        }
    }

    [[eosio::action]]
    void recorddaily(const int64_t &amounttomint ) {
        eosio_assert( has_auth(TREASURYACCOUNT),
                     "missing required authority of fio.treasury");
        if (amounttomint > 0) {
            const uint32_t present_time = now();
            gstaking.staking_rewards_reserves_minted += amounttomint;
            gstaking.combined_token_pool += amounttomint;
            if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
                gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            }
        }
        gstaking.daily_staking_rewards = 0;
    }

    [[eosio::action]]
    void stakefio(const string &fio_address, const int64_t &amount, const int64_t &max_fee,
                         const string &tpid, const name &actor) {
        require_auth(actor);
        const uint32_t present_time = now();
        uint64_t bundleeligiblecountdown = 0;
        FioAddress fa;
        getFioAddressStruct(fio_address, fa);
        fio_400_assert(fio_address == "" || validateFioNameFormat(fa), "fio_address", fio_address, "Invalid FIO Address format",
                       ErrorDomainAlreadyRegistered);

        if (!fio_address.empty()) {
            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);

            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value, ErrorSignature);
            bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
        }

        uint64_t paid_fee_amount = 0;
        bool skipvotecheck = false;
        const uint128_t endpoint_hash = string_to_uint128_hash(STAKE_FIO_TOKENS_ENDPOINT);
        auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);
        fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", STAKE_FIO_TOKENS_ENDPOINT,
                       "FIO fee not found for endpoint", ErrorNoEndpoint);
        const int64_t fee_amount = fee_iter->suf_amount;
        const uint64_t fee_type = fee_iter->type;
        fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                       "unexpected fee type for endpoint stake_fio_tokens, expected 0",
                       ErrorNoEndpoint);
        if (bundleeligiblecountdown > 0) {
            action{
                    permission_level{_self, "active"_n},
                    AddressContract,
                    "decrcounter"_n,
                    make_tuple(fio_address, 1)
            }.send();
        } else {
            paid_fee_amount = fee_iter->suf_amount;
            fio_400_assert(max_fee >= (int64_t)paid_fee_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), STAKE_FIO_TOKENS_ENDPOINT);
            process_rewards(tpid, fee_amount,get_self(), actor);

            if (fee_amount > 0) {
                INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                        ("eosio"_n, {{_self, "active"_n}},
                         {actor, true}
                        );
            }
        }
        //End, bundle eligible fee logic for staking


        if (!tpid.empty()) {
            set_auto_proxy(tpid, 0,get_self(), actor);
            //when a tpid is used, if this is the first call ever for this account to use a tpid,
            //then the auto proxy will be set in an inline action which executes outside of this
            //execution stack. We check if the tpid is a proxy, and if it is then we know that
            //the owner will be auto proxied in this transaction, but in an action outside of this one.
            //so we set a local flag to skip the checks for the "has voted" requirement since we
            //know the owner is auto proxied, this handles the edge condition if the staking is called
            //very early by a new account integrated using tpid.
            FioAddress fa1;
            getFioAddressStruct(tpid, fa1);
            const uint128_t nameHash = string_to_uint128_hash(fa1.fioaddress.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "tpid", fa1.fioaddress,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);
            auto votersbyowner = voters.get_index<"byowner"_n>();
            const auto viter = votersbyowner.find(fioname_iter->owner_account);
            if (viter != votersbyowner.end()) {
                if (viter->is_proxy) {
                    skipvotecheck = true;
                }
            }
        }



        if (!skipvotecheck) {
          auto votersbyowner = voters.get_index<"byowner"_n>();
          auto voter = votersbyowner.find(actor.value);
          fio_400_assert(voter != votersbyowner.end(), "actor",
                       actor.to_string(), "Account has not voted and has not proxied.",ErrorInvalidValue);
          fio_400_assert((((voter->proxy) || (voter->producers.size() > 0) || (voter->is_auto_proxy))),
                           "actor", actor.to_string(), "Account has not voted and has not proxied.", ErrorInvalidValue);
        }

        fio_400_assert(amount > 0, "amount", to_string(amount), "Invalid amount value",ErrorInvalidValue);
        fio_400_assert(max_fee >= 0, "amount", to_string(max_fee), "Invalid fee value",ErrorInvalidValue);
        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,"TPID must be empty or valid FIO address",ErrorPubKeyValid);

        auto stakeablebalance = eosio::token::computeusablebalance(actor,false,false);
        fio_400_assert(stakeablebalance >= (paid_fee_amount + (uint64_t)amount), "amount", to_string(stakeablebalance), "Insufficient balance.",
                       ErrorMaxFeeExceeded);

        if (STAKEFIOTOKENSRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, STAKEFIOTOKENSRAM)
            ).send();
        }

        uint128_t scaled_last_ctp = (uint128_t) gstaking.last_combined_token_pool * STAKING_MULT;
        uint128_t scaled_roe = fiointdivwithrounding(scaled_last_ctp,(uint128_t)gstaking.last_global_srp_count);
        uint128_t scaled_stake_amount = (uint128_t) amount * STAKING_MULT;
        uint128_t srp_128 = fiointdivwithrounding(scaled_stake_amount,scaled_roe);
        uint64_t srpstoaward = (uint64_t) srp_128;

        gstaking.combined_token_pool += amount;
        gstaking.global_srp_count += srpstoaward;
        gstaking.staked_token_pool += amount;

        if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
            gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            gstaking.last_global_srp_count = gstaking.global_srp_count;
        }

        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(actor.value);
        if (astakeiter != astakebyaccount.end()) {
            eosio_assert(astakeiter->account == actor,"incacctstake owner lookup error." );
            astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
                a.total_staked_fio += amount;
                a.total_srp += srpstoaward;
            });
        } else {
            const uint64_t id = accountstaking.available_primary_key();
            accountstaking.emplace(get_self(), [&](struct account_staking_info &p) {
                p.id = id;
                p.account = actor;
                p.total_staked_fio = amount;
                p.total_srp = srpstoaward;
            });
        }

        const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                       to_string(paid_fee_amount) + string("}");

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                       "Transaction is too large", ErrorTransaction);

        send_response(response_string.c_str());
    }


    [[eosio::action]]
    void unstakefio(const string &fio_address,const int64_t &amount, const int64_t &max_fee,
                           const string &tpid, const name &actor) {
        require_auth(actor);

        fio_400_assert(amount > 10000, "amount", to_string(amount), "Invalid amount value",ErrorInvalidValue);
        fio_400_assert(max_fee >= 0, "amount", to_string(max_fee), "Invalid fee value",ErrorInvalidValue);
        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,"TPID must be empty or valid FIO address",ErrorPubKeyValid);

        FioAddress fa;
        getFioAddressStruct(fio_address, fa);

        fio_400_assert(fio_address == "" || validateFioNameFormat(fa), "fio_address", fio_address, "Invalid FIO Address format",
                       ErrorDomainAlreadyRegistered);

        uint64_t bundleeligiblecountdown = 0;

        const uint32_t present_time = now();

        if (!fio_address.empty()) {
            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value, ErrorSignature);
            bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
        }

        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(actor.value);
        eosio_assert(astakeiter != astakebyaccount.end(),"incacctstake, actor has no accountstake record." );
        eosio_assert(astakeiter->account == actor,"incacctstake, actor accountstake lookup error." );
        fio_400_assert(astakeiter->total_staked_fio >= amount, "amount", to_string(amount), "Cannot unstake more than staked.",
                       ErrorInvalidValue);


        uint64_t paid_fee_amount = 0;
        //begin, bundle eligible fee logic for unstaking
        const uint128_t endpoint_hash = string_to_uint128_hash(UNSTAKE_FIO_TOKENS_ENDPOINT);
        auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);
        fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", UNSTAKE_FIO_TOKENS_ENDPOINT,
                       "FIO fee not found for endpoint", ErrorNoEndpoint);
        const int64_t fee_amount = fee_iter->suf_amount;
        const uint64_t fee_type = fee_iter->type;

        fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                       "unexpected fee type for endpoint unstake_fio_tokens, expected 0",
                       ErrorNoEndpoint);

        if (bundleeligiblecountdown > 0) {
            action{
                    permission_level{_self, "active"_n},
                    AddressContract,
                    "decrcounter"_n,
                    make_tuple(fio_address, 1)
            }.send();
        } else {
            paid_fee_amount = fee_iter->suf_amount;
            fio_400_assert(max_fee >= (int64_t)paid_fee_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), UNSTAKE_FIO_TOKENS_ENDPOINT);
            process_rewards(tpid, fee_amount,get_self(), actor);

            if (fee_amount > 0) {
                INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                        ("eosio"_n, {{_self, "active"_n}},
                         {actor, true}
                        );
            }
        }
        //End, bundle eligible fee logic for staking

        auto usablebalance = eosio::token::computeusablebalance(actor,false,false);

        //if the usable balance is greater than the fee, we are clear of affects of
        //fees.
        //else if the usable balance is less than the amount of the fee, this is an error
        // cannot unstake, insufficient funds for unstake operation.
        fio_400_assert(usablebalance >= paid_fee_amount, "amount", to_string(usablebalance), "Insufficient funds to cover fee",
                       ErrorMaxFeeExceeded);

        //RAM bump
        if (UNSTAKEFIOTOKENSRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, UNSTAKEFIOTOKENSRAM)
            ).send();
        }

        uint64_t srps_this_unstake;

        if (amount == astakeiter->total_staked_fio) {
            srps_this_unstake = astakeiter->total_srp; // If all SUFs then all SRPs
        } else {
            uint128_t scaled_unstake = (uint128_t) amount * STAKING_MULT; // unstake sufs are multiplied by mult and stored as interim variable
            //what percentage of the SRPs held by this user is associated with this unstake.
            uint128_t scaled_user_share_srps = fiointdivwithrounding( (uint128_t) scaled_unstake, (uint128_t) astakeiter->total_staked_fio );// the interim variable is divided by staked sufs to get upscaled share of srp
            uint128_t scaled_srps_unstake = scaled_user_share_srps * (uint128_t) astakeiter->total_srp; // user's SRPs are multiplied by upscaled share of SUFs being unstaked to produce upscaled SRPs to unstake
            uint128_t srps_this_unstake_128 = fiointdivwithrounding(scaled_srps_unstake,STAKING_MULT);// SRPs are downscaled by dividing by multiplie
            srps_this_unstake = (uint64_t) srps_this_unstake_128; // This just converts from uint128 to uint64. Not sure if this is needed
        }


        uint64_t totalsufsthisunstake;
        uint128_t interim_usrplctp = (uint128_t) srps_this_unstake * (uint128_t) gstaking.last_combined_token_pool; // SRPs being unstaked are multiplied by LCTP first
        uint128_t got_suf_big = fiointdivwithrounding(interim_usrplctp, (uint128_t) gstaking.last_global_srp_count); // Then are divided by LGSRP
        totalsufsthisunstake = (uint64_t) got_suf_big;
        //Replace the current assertion with:
        //If the number of sufs rewarded is less than the number of sufs unstaked
        //And the difference is less than 1000 sufs
        //Give the user the number of sufs they unstaked.
        if( totalsufsthisunstake < amount ){
            eosio_assert((amount - totalsufsthisunstake) < 1000,
                         "unstakefio, total sufs this unstake is 1000 or more sufs less than amount unstaked.");
            totalsufsthisunstake = amount;
        }
        uint64_t totalrewardamount = totalsufsthisunstake - amount;
        uint64_t tenpercent = fiointdivwithrounding(totalrewardamount,(uint64_t) 10);
        uint64_t stakingrewardamount = totalrewardamount - tenpercent;
        uint64_t tpidrewardamount = tenpercent;


        eosio_assert(astakeiter->total_srp >= srps_this_unstake,"unstakefio, total srp for account must be greater than or equal srps_this_unstake." );
        eosio_assert(astakeiter->total_staked_fio >= amount,"unstakefio, total staked fio for account must be greater than or equal fiostakedsufs." );

        astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
            a.total_staked_fio -= amount;
            a.total_srp -= srps_this_unstake;
        });

        //transfer the staking reward amount.
        if (stakingrewardamount > 0) {
            action(permission_level{get_self(), "active"_n},
                   TREASURYACCOUNT, "paystake"_n,
                   make_tuple(actor, stakingrewardamount)
            ).send();
        }

        uint64_t totalamount_unstaking = (amount+stakingrewardamount);

        eosio_assert(gstaking.combined_token_pool >= totalamount_unstaking,"unstakefio, combined token pool must be greater or equal to amount plus stakingrewardamount. " );
        eosio_assert(gstaking.staked_token_pool >= amount,"unstakefio, staked token pool must be greater or equal to staked amount. " );
        eosio_assert(gstaking.global_srp_count >= srps_this_unstake,"unstakefio, global srp count must be greater or equal to srps_this_unstake. " );


        gstaking.combined_token_pool -= totalamount_unstaking;
        gstaking.staked_token_pool -= amount;
        gstaking.global_srp_count -= srps_this_unstake;

        if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
            gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            gstaking.last_global_srp_count = gstaking.global_srp_count;
        }

        if ((tpid.length() > 0)&&(tpidrewardamount>0)){
            const uint128_t tnameHash = string_to_uint128_hash(tpid.c_str());
            auto tnamesbyname = fionames.get_index<"byname"_n>();
            auto tfioname_iter = tnamesbyname.find(tnameHash);
            fio_400_assert(tfioname_iter != tnamesbyname.end(), "fio_address", tpid,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);
            action(
                    permission_level{get_self(), "active"_n},
                    TPIDContract,
                    "updatetpid"_n,
                    std::make_tuple(tpid, actor, tpidrewardamount)
            ).send();

            eosio_assert(tpidrewardamount<= gstaking.combined_token_pool,"unstakefio, tpidrewardamount must be less or equal to state combined token pool." );
            gstaking.combined_token_pool -= tpidrewardamount;
            if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
                gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            }
        }

        //7 days unstaking lock duration.
        int64_t UNSTAKELOCKDURATIONSECONDS = 604800;


        auto locks_by_owner = generallocks.get_index<"byowner"_n>();
        auto lockiter = locks_by_owner.find(actor.value);
        if (lockiter != locks_by_owner.end()) {
            //if they have general locks then adapt the locks.
            int64_t newlockamount = lockiter->lock_amount + (stakingrewardamount + amount);
            int64_t newremaininglockamount = lockiter->remaining_lock_amount + (stakingrewardamount + amount);
            uint32_t insertperiod = (present_time - lockiter->timestamp) + UNSTAKELOCKDURATIONSECONDS;
            uint32_t insertday = (lockiter->timestamp + insertperiod) / SECONDSPERDAY;
            uint32_t expirednowduration = present_time - lockiter->timestamp;
            uint32_t payouts = lockiter->payouts_performed;
            vector <eosiosystem::lockperiodv2> newperiods;

            bool insertintoexisting = false;
            uint32_t lastperiodduration = 0;
            int insertindex = -1;
            uint32_t daysforperiod = 0;
            bool foundinsix = false;

            for (int i = 0; i < lockiter->periods.size(); i++) {
                daysforperiod = (lockiter->timestamp + lockiter->periods[i].duration)/SECONDSPERDAY;

                uint64_t amountthisperiod = lockiter->periods[i].amount;
                //only set the insertindex on the first one greater than or equal that HAS NOT been paid out.
                if ((daysforperiod >= insertday) && !foundinsix && (i > (int)lockiter->payouts_performed-1)) {
                    insertindex = newperiods.size();
                    //always insert into the same day.
                   if (daysforperiod == insertday) {
                        insertintoexisting = true;
                        amountthisperiod += (stakingrewardamount + amount);
                    }
                    foundinsix = true;
                }
                lastperiodduration = lockiter->periods[i].duration;
                eosiosystem::lockperiodv2 tperiod;
                tperiod.duration = lockiter->periods[i].duration;
                tperiod.amount = amountthisperiod;

                //only those periods not in the past go into the list of periods.
                //remove old periods. be sure to adapt the lock information correctly when
                //removing locking periods, sometimes the lock period has not been paid, but
                //is in the past and gets removed.
                if( tperiod.duration >= expirednowduration) {
                    newperiods.push_back(tperiod);
                }else{ //we are not placing into the result list, so adapt the lock info.
                    newlockamount -= tperiod.amount;
                    if((newlockamount < newremaininglockamount)&&(payouts == 0)){
                         //if we are removing a lock period that has not been paid out yet adapt the remaining lock amount.
                         newremaininglockamount = newlockamount;
                    }
                    else{
                         string msg = "unstakefio, inconsistent general lock state lock amount "+ to_string(newlockamount) +" less than remaining lock amount. "+ to_string(newremaininglockamount);
                         //this check is here for code safety. if there were payouts left we should never see newlockamount < newremaininglockamount
                         eosio_assert(newlockamount >= newremaininglockamount,msg.c_str() );
                    }
                    if(payouts >0) {
                        payouts--;
                    }
                }
            }

            //add the period to the list.
            if (!insertintoexisting) {
                eosiosystem::lockperiodv2 iperiod;
                iperiod.duration = insertperiod;
                iperiod.amount = amount + stakingrewardamount;
                if (insertindex == -1) {
                    //insert at the end of the list, my duration is greater than all in the list.
                    newperiods.push_back(iperiod);
                }else {
                        newperiods.insert(newperiods.begin() + insertindex, iperiod);
                }
            }

            //BD-3941 begin, be sure to handle edge case where we have locks and all are in the past.
            if (foundinsix || newperiods.size() > 1) {
                action(
                        permission_level{get_self(), "active"_n},
                        SYSTEMACCOUNT,
                        "modgenlocked"_n,
                        std::make_tuple(actor, newperiods, newlockamount, newremaininglockamount, payouts)
                ).send();
            }
            else {

                print("EDEDEDEDEDEDEDEDEDEDEDEDEDEDEDEDEDEDEDEDED adding new when one already exists for account ",actor.to_string(),"\n");
                    bool canvote = true;
                    int64_t lockamount = (int64_t)(stakingrewardamount + amount);

                    vector <eosiosystem::lockperiodv2> periods;
                    eosiosystem::lockperiodv2 period;
                    period.duration = UNSTAKELOCKDURATIONSECONDS;
                    period.amount = lockamount;
                    periods.push_back(period);
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, addgenlocked)
                            ("eosio"_n, {{_self, "active"_n}},
                             {actor, periods, canvote, lockamount}
                            );

            }
            //BD-3941 end
        }else {
            //else make new lock.
            bool canvote = true;
            int64_t lockamount = (int64_t)(stakingrewardamount + amount);

            vector <eosiosystem::lockperiodv2> periods;
            eosiosystem::lockperiodv2 period;
            period.duration = UNSTAKELOCKDURATIONSECONDS;
            period.amount = lockamount;
            periods.push_back(period);
            INLINE_ACTION_SENDER(eosiosystem::system_contract, addgenlocked)
                    ("eosio"_n, {{_self, "active"_n}},
                     {actor, periods, canvote, lockamount}
                    );
        }

        const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                       to_string(paid_fee_amount) + string("}");

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                       "Transaction is too large", ErrorTransaction);

        send_response(response_string.c_str());
    }

};     //class Staking

EOSIO_DISPATCH(Staking, (stakefio)(unstakefio)(incgrewards)(recorddaily) )
}
