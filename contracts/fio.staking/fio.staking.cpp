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

namespace fioio {

class [[eosio::contract("Staking")]]  Staking: public eosio::contract {

private:

        global_staking_singleton         staking;
        global_staking_state             gstaking;
        account_staking_table            accountstaking;
        eosiosystem::voters_table        voters;
        fionames_table                   fionames;
        fiofee_table                     fiofees;
        eosiosystem::general_locks_table_v2 generallocks;
        bool debugout = false;

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



    //incgrewards performs the staking state increments when rewards are identified (including minted) during fee collection.
    //  params
    //      fioamountsufs, this is the amount of FIO being added to the rewards (from fees or when minted). units SUFs
    [[eosio::action]]
    void incgrewards(const int64_t &fioamountsufs ) {
        eosio_assert((has_auth(AddressContract) || has_auth(TokenContract) || has_auth(TREASURYACCOUNT) ||
                      has_auth(STAKINGACCOUNT) ||  has_auth(REQOBTACCOUNT) || has_auth(SYSTEMACCOUNT) || has_auth(FeeContract)),
                     "missing required authority of fio.address, fio.treasury, fio.fee, fio.token, fio.stakng, eosio or fio.reqobt");
        gstaking.rewards_token_pool += fioamountsufs;
        gstaking.daily_staking_rewards += fioamountsufs;
        gstaking.combined_token_pool += fioamountsufs;
     }

     //recorddaily will perform the daily update of global state, when bps claim rewards.
    [[eosio::action]]
    void recorddaily(const int64_t &amounttomint ) {
        eosio_assert( has_auth(TREASURYACCOUNT),
                     "missing required authority of fio.treasury");
        if (amounttomint > 0) {
            gstaking.staking_rewards_reserves_minted += amounttomint;
            gstaking.daily_staking_rewards += amounttomint;
        }
        gstaking.combined_token_pool += gstaking.daily_staking_rewards;
        gstaking.daily_staking_rewards = 0;
    }

    //incgstkmint increments the staking_rewards_reserves_minted
    [[eosio::action]]
    void stakefio(const string &fio_address, const int64_t &amount, const int64_t &max_fee,
                         const string &tpid, const name &actor) {
        //signer not actor.
        require_auth(actor);
       
        //check if the actor has voted.
        auto votersbyowner = voters.get_index<"byowner"_n>();
        auto voter = votersbyowner.find(actor.value);
        fio_400_assert(voter != votersbyowner.end(), "actor",
                actor.to_string(), "Account has not voted and has not proxied.",ErrorInvalidValue);
        //if they are in the table check if they are is_auto_proxy, or if they have a proxy, or if they have producers not empty
        fio_400_assert((((voter->proxy) || (voter->producers.size() > 0) || (voter->is_auto_proxy))),
                "actor", actor.to_string(), "Account has not voted and has not proxied.",ErrorInvalidValue);

        fio_400_assert(amount > 0, "amount", to_string(amount), "Invalid amount value",ErrorInvalidValue);
        fio_400_assert(max_fee >= 0, "amount", to_string(max_fee), "Invalid fee value",ErrorInvalidValue);
        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,"TPID must be empty or valid FIO address",ErrorPubKeyValid);

        uint64_t bundleeligiblecountdown = 0;
        //process the fio address specified
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

            const uint32_t expiration = fioname_iter->expiration;
            const uint32_t present_time = now();
            fio_400_assert(present_time <= expiration, "fio_address", fio_address, "FIO Address expired. Renew first.",
                           ErrorDomainExpired);
            bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
        }

        //get the usable balance for the account
        //this is account balance - genesis locked tokens - general locked balance.
       // print("EDEDEDEDEDEDEDEDEDED calling from staking!!!!!!!!!");
        auto stakeablebalance = eosio::token::computeusablebalance(actor,false);


        uint64_t paid_fee_amount = 0;
        //begin, bundle eligible fee logic for staking
        const uint128_t endpoint_hash = string_to_uint128_hash(STAKE_FIO_TOKENS_ENDPOINT);

        auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);

        //if the fee isnt found for the endpoint, then 400 error.
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

        fio_400_assert(stakeablebalance >= (paid_fee_amount + (uint64_t)amount), "max_fee", to_string(max_fee), "Insufficient balance.",
                       ErrorMaxFeeExceeded);
        //End, bundle eligible fee logic for staking

        //RAM bump
        if (STAKEFIOTOKENSRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, STAKEFIOTOKENSRAM)
            ).send();
        }


        //compute rate of exchange and SRPs
        uint64_t rateofexchange =  1;
        if (gstaking.combined_token_pool >= COMBINEDTOKENPOOLMINIMUM) {
            print("EDEDEDEDEDEDEDEDEDEDED global srp count ", gstaking.global_srp_count);
            print("EDEDEDEDEDEDEDEDEDEDED combined_token_pool ", gstaking.combined_token_pool);
            rateofexchange = gstaking.combined_token_pool / gstaking.global_srp_count;
            print("EDEDEDEDEDEDEDEDEDEDED rate of exchange set to ", rateofexchange);
            if(rateofexchange < 1) {
                print("EDEDEDEDEDEDEDEDEDEDED RATE OF EXCHANGE LESS THAN 1 ", rateofexchange);
                rateofexchange = 1;
            }
        }

        uint64_t srptoaward = amount / rateofexchange;

        //update global staking state
        gstaking.combined_token_pool += amount;
        gstaking.global_srp_count += srptoaward;
        gstaking.staked_token_pool += amount;


        //update account staking info
        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(actor.value);
        if (astakeiter != astakebyaccount.end()) {
            eosio_assert(astakeiter->account == actor,"incacctstake owner lookup error." );
            //update the existing record
            astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
                a.total_staked_fio += amount;
                a.total_srp += srptoaward;
            });
        } else {
            const uint64_t id = accountstaking.available_primary_key();
            accountstaking.emplace(get_self(), [&](struct account_staking_info &p) {
                p.id = id;
                p.account = actor;
                p.total_staked_fio = amount;
                p.total_srp = srptoaward;
            });
        }
        //end increment account staking info
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


        //process the fio address specified
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

            const uint32_t expiration = fioname_iter->expiration;

            fio_400_assert(present_time <= expiration, "fio_address", fio_address, "FIO Address expired. Renew first.",
                           ErrorDomainExpired);
            bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
        }

        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(actor.value);
        eosio_assert(astakeiter != astakebyaccount.end(),"incacctstake, actor has no accountstake record." );
        eosio_assert(astakeiter->account == actor,"incacctstake, actor accountstake lookup error." );
        fio_400_assert(astakeiter->total_staked_fio >= amount, "amount", to_string(amount), "Cannot unstake more than staked.",
                       ErrorInvalidValue);

        //get the usable balance for the account
        //this is account balance - genesis locked tokens - general locked balance.
       // print("EDEDEDEDEDEDEDEDEDED calling from unstaking!!!!!!!!!");
        auto stakeablebalance = eosio::token::computeusablebalance(actor,false);

        uint64_t paid_fee_amount = 0;
        //begin, bundle eligible fee logic for staking
        const uint128_t endpoint_hash = string_to_uint128_hash(UNSTAKE_FIO_TOKENS_ENDPOINT);

        auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);

        //if the fee isnt found for the endpoint, then 400 error.
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

        //fio_400_assert(stakeablebalance >= (paid_fee_amount + (uint64_t)amount), "max_fee", to_string(max_fee), "Insufficient balance.",
         //              ErrorMaxFeeExceeded);
        //End, bundle eligible fee logic for staking

        //RAM bump
        if (UNSTAKEFIOTOKENSRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, UNSTAKEFIOTOKENSRAM)
            ).send();
        }
        //SRPs to Claim are computed: Staker's Account SRPs * (Unstaked amount / Total Tokens Staked in Staker's Account)
         //                                             this needs to be a floating point (double) operation
       //round this to avoid issues with decimal representations
        uint64_t srpstoclaim = (uint64_t)(((double)astakeiter->total_srp * (double)( (double)amount / (double)astakeiter->total_staked_fio))+0.5);

        //print("EDEEEDEDEDEDEDED srps to claim is ",to_string(srpstoclaim),"\n");
        //compute rate of exchange
        uint64_t rateofexchange =  1;
        if (gstaking.combined_token_pool >= COMBINEDTOKENPOOLMINIMUM) {
            print("EDEDEDEDEDEDEDEDEDEDED global srp count ", gstaking.global_srp_count);
            print("EDEDEDEDEDEDEDEDEDEDED combined_token_pool ", gstaking.combined_token_pool);
            rateofexchange = gstaking.combined_token_pool / gstaking.global_srp_count;
            print("EDEDEDEDEDEDEDEDEDEDED rate of exchange set to ", rateofexchange);
            if(rateofexchange < 1) {
                print("EDEDEDEDEDEDEDEDEDEDED RATE OF EXCHANGE LESS THAN 1 ", rateofexchange);
                rateofexchange = 1;
            }
        }

        eosio_assert((srpstoclaim * rateofexchange) >= amount, "unstakefio, invalid calc in totalrewardamount, must be that (srpstoclaim * rateofexchange) > amount. ");
        uint64_t totalrewardamount = ((srpstoclaim * rateofexchange) - amount);
        print("EDEDEDEDEDEDEDEDEDEDED total reward amount is ", totalrewardamount);
        uint64_t tenpercent = totalrewardamount / 10;
        //Staking Reward Amount is computed: ((SRPs to Claim * Rate of Exchnage) - Unstake amount) * 0.9
        uint64_t stakingrewardamount = tenpercent * 9;
        print("EDEDEDEDEDEDEDEDEDEDED staking reward amount is ", totalrewardamount);
        // TPID Reward Amount is computed: ((SRPs to Claim * Rate of Exchnage) - Unstake amount) * 0.1
        uint64_t tpidrewardamount = tenpercent;

        //decrement staking by account.
        eosio_assert(astakeiter->total_srp >= srpstoclaim,"unstakefio, total srp for account must be greater than or equal srpstoclaim." );
        eosio_assert(astakeiter->total_staked_fio >= amount,"unstakefio, total staked fio for account must be greater than or equal fiostakedsufs." );

        //update the existing record
        astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
            a.total_staked_fio -= amount;
            a.total_srp -= srpstoclaim;
        });

        //transfer the staking reward amount.
        if (stakingrewardamount > 0) {
            //Staking Reward Amount is transferred to Staker's Account.
            //           Memo: "Paying Staking Rewards"
            action(permission_level{get_self(), "active"_n},
                   TREASURYACCOUNT, "paystake"_n,
                   make_tuple(actor, stakingrewardamount)
            ).send();
        }

        //decrement the global state
        //avoid overflows due to negative results.
        eosio_assert(gstaking.combined_token_pool >= (amount+stakingrewardamount),"unstakefio, combined token pool must be greater or equal to amount plus stakingrewardamount. " );
        eosio_assert(gstaking.staked_token_pool >= amount,"unstakefio, staked token pool must be greater or equal to staked amount. " );
        eosio_assert(gstaking.global_srp_count >= srpstoclaim,"unstakefio, global srp count must be greater or equal to srpstoclaim. " );

        //     decrement the combined_token_pool by fiostaked+fiorewarded.
        gstaking.combined_token_pool -= (amount+stakingrewardamount);
        //     decrement the staked_token_pool by fiostaked.
        gstaking.staked_token_pool -= amount;
        //     decrement the global_srp_count by srpcount.
        gstaking.global_srp_count -= srpstoclaim;

        //pay the tpid.
        if ((tpid.length() > 0)&&(tpidrewardamount>0)){
            //get the owner of the tpid and pay them.
            const uint128_t tnameHash = string_to_uint128_hash(tpid.c_str());
            auto tnamesbyname = fionames.get_index<"byname"_n>();
            auto tfioname_iter = tnamesbyname.find(tnameHash);
            fio_400_assert(tfioname_iter != tnamesbyname.end(), "fio_address", tpid,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);

            const uint32_t expiration = tfioname_iter->expiration;
            fio_400_assert(present_time <= expiration, "fio_address", fio_address, "FIO Address expired. Renew first.",
                           ErrorDomainExpired);

            //pay the tpid
            action(
                    permission_level{get_self(), "active"_n},
                    TPIDContract,
                    "updatetpid"_n,
                    std::make_tuple(tpid, actor, tpidrewardamount)
            ).send();

            //decrement the amount paid from combined token pool.
            if(tpidrewardamount<= gstaking.combined_token_pool) {
                gstaking.combined_token_pool -= tpidrewardamount;
            }
        }


        //look and see if they have any general locks.
        auto locks_by_owner = generallocks.get_index<"byowner"_n>();
        auto lockiter = locks_by_owner.find(actor.value);
        if (lockiter != locks_by_owner.end()) {
            //if they have general locks then adapt the locks.
            //get the amount of the lock.
            int64_t newlockamount = lockiter->lock_amount + (stakingrewardamount + amount);
            //get the remaining unlocked of the lock.
            int64_t newremaininglockamount = lockiter->remaining_lock_amount + (stakingrewardamount + amount);
            //get the timestamp of the lock.
            uint32_t insertperiod = (present_time - lockiter->timestamp) + 604800;
            //the days since launch.
            uint32_t insertday = (lockiter->timestamp + insertperiod) / SECONDSPERDAY;
            //if your duration is less than this the period is in the past.
            uint32_t expirednowduration = present_time - lockiter->timestamp;
            uint32_t payouts = lockiter->payouts_performed;


            vector <eosiosystem::lockperiodv2> newperiods;

            bool insertintoexisting = false;
            uint32_t lastperiodduration = 0;
            int insertindex = -1;
            uint32_t daysforperiod = 0;

            for (int i = 0; i < lockiter->periods.size(); i++) {
                daysforperiod = (lockiter->timestamp + lockiter->periods[i].duration)/SECONDSPERDAY;
                uint64_t amountthisperiod = lockiter->periods[i].amount;
                if (daysforperiod >= insertday) {
                    insertindex = newperiods.size();
                    //always insert into the same day.
                    if (daysforperiod == insertday) {
                        insertintoexisting = true;
                        amountthisperiod += (stakingrewardamount + amount);
                    }
                }
                lastperiodduration = lockiter->periods[i].duration;
                eosiosystem::lockperiodv2 tperiod;
                tperiod.duration = lockiter->periods[i].duration;
                tperiod.amount = amountthisperiod;

                //only those periods not in the past go into the list of periods.
                //remove old periods.
                if( tperiod.duration >= expirednowduration) {
                    newperiods.push_back(tperiod);
                }else{
                    eosio_assert(payouts > 0 ,"unstakefio,  internal error decrementing payouts. " );
                    newlockamount -= tperiod.amount;
                    eosio_assert(newlockamount >= newremaininglockamount,"unstakefio, inconsistent general lock state lock amount less than remaining lock amount. " );
                    payouts --;
                }
            }


            //add the period to the list.
            if (!insertintoexisting) {
               // print("EDEDEDEDEDEDEDEDED totalnewpercent ",totalnewpercent,"\n");
                eosiosystem::lockperiodv2 iperiod;
                iperiod.duration = insertperiod;
                iperiod.amount = amount;
                newperiods.insert(newperiods.begin() + insertindex, iperiod);
            }

           //update the locks table..   modgenlocked
            action(
                    permission_level{get_self(), "active"_n},
                    SYSTEMACCOUNT,
                    "modgenlocked"_n,
                    std::make_tuple(actor, newperiods, newlockamount, newremaininglockamount, payouts)
            ).send();
        }else {
            //else make new lock.
            bool canvote = true;
            int64_t lockamount = (int64_t)(stakingrewardamount + amount);
            print("EDEDEDEDEDEDEDEDED creating general lock for amount ", lockamount, "\n");
            vector <eosiosystem::lockperiodv2> periods;
            eosiosystem::lockperiodv2 period;
            period.duration = 604800;
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
