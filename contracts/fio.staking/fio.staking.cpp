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

#define ENABLESTAKINGREWARDSEPOCHSEC  1627686000  //July 30 5:00PM MST 11:00PM GMT

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
        //debug output flag
        bool debugout = true;

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
                      has_auth(STAKINGACCOUNT) ||  has_auth(REQOBTACCOUNT) || has_auth(SYSTEMACCOUNT) ||
                      has_auth(FeeContract) || has_auth(EscrowContract)),
                     "missing required authority of fio.address, fio.treasury, fio.fee, fio.token, fio.staking, fio.escrow, eosio or fio.reqobt");

        const uint32_t present_time = now();

        gstaking.rewards_token_pool += fioamountsufs;
        gstaking.daily_staking_rewards += fioamountsufs;
        gstaking.combined_token_pool += fioamountsufs;

        if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
            //update the last values in state for ctp and gsrp.
            gstaking.last_combined_token_pool = gstaking.combined_token_pool;
        }

        if (debugout) {
            print(" rewards token pool incremented to ", gstaking.rewards_token_pool,"\n");
            print(" daily staking rewards incremented to ", gstaking.daily_staking_rewards,"\n");
            print(" combined_token_pool incremented to ", gstaking.combined_token_pool,"\n");
        }
     }

     //recorddaily will perform the daily update of global state, when bps claim rewards.
    [[eosio::action]]
    void recorddaily(const int64_t &amounttomint ) {
        eosio_assert( has_auth(TREASURYACCOUNT),
                     "missing required authority of fio.treasury");
        if (amounttomint > 0) {
            const uint32_t present_time = now();
            gstaking.staking_rewards_reserves_minted += amounttomint;
            gstaking.combined_token_pool += amounttomint;
            if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
                //update the last values in state for ctp and gsrp.
                gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            }
        }
        gstaking.daily_staking_rewards = 0;
        if(debugout){
            print("recorddaily completed successfully.","\n");
        }
    }

    //this action performs staking of fio tokens
    [[eosio::action]]
    void stakefio(const string &fio_address, const int64_t &amount, const int64_t &max_fee,
                         const string &tpid, const name &actor) {
        //signer not actor.
        require_auth(actor);
        const uint32_t present_time = now();

        if(debugout) {
            print(" calling stakefio fio address ", fio_address,"\n");
        }

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

            fio_400_assert(present_time <= expiration, "fio_address", fio_address, "FIO Address expired. Renew first.",
                           ErrorDomainExpired);
            bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
        }


        uint64_t paid_fee_amount = 0;
        bool skipvotecheck = false;
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

            if (!tpid.empty()) {
                if (debugout) {
                    print(" calling process auto proxy with ", tpid,"\n");
                }
                set_auto_proxy(tpid, 0,get_self(), actor);

                //when a tpid is used, if this is the first call for this account to use a tpid,
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
                //now use the owner to find the voting record.
                auto votersbyowner = voters.get_index<"byowner"_n>();
                const auto viter = votersbyowner.find(fioname_iter->owner_account);
                if (viter != votersbyowner.end()) {
                    if (viter->is_proxy) {
                        skipvotecheck = true;
                    }
                }
            }

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

        //if we are not auto proxying for the first time, check if the actor has voted.
        if (!skipvotecheck) {
          auto votersbyowner = voters.get_index<"byowner"_n>();
          auto voter = votersbyowner.find(actor.value);
          fio_400_assert(voter != votersbyowner.end(), "actor",
                       actor.to_string(), "Account has not voted and has not proxied.",ErrorInvalidValue);
          //if they are in the table check if they are is_auto_proxy, or if they have a proxy, or if they have producers not empty
          fio_400_assert((((voter->proxy) || (voter->producers.size() > 0) || (voter->is_auto_proxy))),
                           "actor", actor.to_string(), "Account has not voted and has not proxied.", ErrorInvalidValue);
        }


        fio_400_assert(amount > 0, "amount", to_string(amount), "Invalid amount value",ErrorInvalidValue);
        fio_400_assert(max_fee >= 0, "amount", to_string(max_fee), "Invalid fee value",ErrorInvalidValue);
        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,"TPID must be empty or valid FIO address",ErrorPubKeyValid);



        //get the usable balance for the account
        auto stakeablebalance = eosio::token::computeusablebalance(actor,false);
        fio_400_assert(stakeablebalance >= (paid_fee_amount + (uint64_t)amount), "max_fee", to_string(max_fee), "Insufficient balance.",
                       ErrorMaxFeeExceeded);

        //RAM bump
        if (STAKEFIOTOKENSRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, STAKEFIOTOKENSRAM)
            ).send();
        }


        //begin new logic for state management.

        //what is this mult used for.
        const uint128_t multiplier = 1000000000000000000;

        uint128_t scaled_last_ctp = (uint128_t) gstaking.last_combined_token_pool * multiplier;
        uint128_t scaled_roe = fiointdivwithrounding(scaled_last_ctp,(uint128_t)gstaking.last_global_srp_count);

        uint128_t scaled_stake_amount = (uint128_t) amount * multiplier;
        uint128_t srp_128 = fiointdivwithrounding(scaled_stake_amount,scaled_roe);
        uint64_t srpstoaward = (uint64_t) srp_128;

        //update global staking state
        gstaking.combined_token_pool += amount;
        gstaking.global_srp_count += srpstoaward;
        gstaking.staked_token_pool += amount;

        if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
            //update the last values in state for ctp and gsrp.
            gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            gstaking.last_global_srp_count = gstaking.global_srp_count;
        }

        //end new logic for state management.


        if(debugout){
            print("the value of combined_token_pool after state inc in stake: ",gstaking.combined_token_pool,"\n" );
        }



        //update account staking info
        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(actor.value);
        if (astakeiter != astakebyaccount.end()) {
            eosio_assert(astakeiter->account == actor,"incacctstake owner lookup error." );
            //update the existing record
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
        //end increment account staking info



        const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                       to_string(paid_fee_amount) + string("}");

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                       "Transaction is too large", ErrorTransaction);

        send_response(response_string.c_str());
    }


    //this action performs the unstaking of fio tokens.
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
        auto stakeablebalance = eosio::token::computeusablebalance(actor,false);

        uint64_t paid_fee_amount = 0;
        //begin, bundle eligible fee logic for unstaking
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
                if (debugout) {
                    print("collecting fee amount ", fee_amount, "\n");
                }
                INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                        ("eosio"_n, {{_self, "active"_n}},
                         {actor, true}
                        );
            }
        }
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



        //new new staking state integration

        if(debugout) {
            print("account total staked fio is ",  astakeiter->total_staked_fio,"\n");
            print("account total srp is ",  astakeiter->total_staked_fio,"\n");
        }


        uint64_t srps_this_unstake;
        const uint128_t multiplier = 1000000000000000000;

        if (amount == astakeiter->total_staked_fio) {
            srps_this_unstake = astakeiter->total_srp; // If all SUFs then all SRPs
        } else {
            uint128_t scaled_unstake = (uint128_t) amount * multiplier; // unstake sufs are multiplied by mult and stored as interim variable
            //what percentage of the SRPs held by this user is associated with this unstake.
            uint128_t scaled_user_share_srps = fiointdivwithrounding( (uint128_t) scaled_unstake, (uint128_t) astakeiter->total_staked_fio );// the interim variable is divided by staked sufs to get upscaled share of srp
            uint128_t scaled_srps_unstake = scaled_user_share_srps * (uint128_t) astakeiter->total_srp; // user's SRPs are multiplied by upscaled share of SUFs being unstaked to produce upscaled SRPs to unstake
            uint128_t srps_this_unstake_128 = fiointdivwithrounding(scaled_srps_unstake,multiplier);// SRPs are downscaled by dividing by multiplie
            srps_this_unstake = (uint64_t) srps_this_unstake_128; // This just converts from uint128 to uint64. Not sure if this is needed
        }

        //end new new staking integration


        //revise
        const string message = "unstakefio, srps to claim "+ to_string(srps_this_unstake)  +
                               " srps_this_unstake " + to_string(srps_this_unstake) + " amount "+ to_string(amount) +
                               " srps_this_unstake must be >= amount. must be greater than or equal srps_this_unstake ";
        if (debugout) {
            print(message, "\n");
        }

        //add code to compute sufs!!!!!
        uint64_t totalsufsthisunstake;

        if(debugout) {
            print("last gsrp is ",  gstaking.last_global_srp_count,"\n");
            print("last combined token pool is ",  gstaking.last_combined_token_pool,"\n");
        }

        uint128_t interim_usrplctp = (uint128_t) srps_this_unstake * (uint128_t) gstaking.last_combined_token_pool; // SRPs being unstaked are multiplied by LCTP first
        uint128_t got_suf_big = fiointdivwithrounding(interim_usrplctp, (uint128_t) gstaking.last_global_srp_count); // Then are divided by LGSRP
        totalsufsthisunstake = (uint64_t) got_suf_big; // This just converts from uint128 to uint64. Not sure if this is needed

        uint64_t totalrewardamount = totalsufsthisunstake - amount;

        const char* mptr = &message[0];
        eosio_assert(srps_this_unstake >= amount, mptr);
        //revise

        if(debugout) {
            print("total reward amount is ", totalrewardamount,"\n");
        }
        uint64_t tenpercent = totalrewardamount / 10;
        //Staking Reward Amount is computed: ((SRPs to Claim * Rate of Exchnage) - Unstake amount) * 0.9
        uint64_t stakingrewardamount = tenpercent * 9;
        if(debugout) {
            print(" staking reward amount is ", stakingrewardamount,"\n");
        }
        // TPID Reward Amount is computed: ((SRPs to Claim * Rate of Exchnage) - Unstake amount) * 0.1
        uint64_t tpidrewardamount = tenpercent;



        //decrement staking by account.
        eosio_assert(astakeiter->total_srp >= srps_this_unstake,"unstakefio, total srp for account must be greater than or equal srps_this_unstake." );
        eosio_assert(astakeiter->total_staked_fio >= amount,"unstakefio, total staked fio for account must be greater than or equal fiostakedsufs." );

        //update the existing record
        astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
            a.total_staked_fio -= amount;
            a.total_srp -= srps_this_unstake;
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
        uint64_t totalamount_unstaking = (amount+stakingrewardamount);

        eosio_assert(gstaking.combined_token_pool >= totalamount_unstaking,"unstakefio, combined token pool must be greater or equal to amount plus stakingrewardamount. " );
        eosio_assert(gstaking.staked_token_pool >= amount,"unstakefio, staked token pool must be greater or equal to staked amount. " );
        eosio_assert(gstaking.global_srp_count >= srps_this_unstake,"unstakefio, global srp count must be greater or equal to srps_this_unstake. " );

        //     decrement the combined_token_pool by fiostaked+fiorewarded.
        gstaking.combined_token_pool -= totalamount_unstaking;
        //     decrement the staked_token_pool by fiostaked.
        gstaking.staked_token_pool -= amount;
        //     decrement the global_srp_count by srpcount.
        gstaking.global_srp_count -= srps_this_unstake;

        //new staking state integration
        if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
            //update the last values in state for ctp and gsrp.
            gstaking.last_combined_token_pool = gstaking.combined_token_pool;
            gstaking.last_global_srp_count = gstaking.global_srp_count;
        }
        //new staking state integration


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
                if ((gstaking.staked_token_pool >= STAKEDTOKENPOOLMINIMUM) && (present_time > ENABLESTAKINGREWARDSEPOCHSEC)) {
                    //update the last values in state for ctp and gsrp.
                    gstaking.last_combined_token_pool = gstaking.combined_token_pool;
                }
            }
        }

        //7 days unstaking lock duration.
        int64_t UNSTAKELOCKDURATIONSECONDS = 604800;
       //TEST CODE DO NOT DELIVER, unstake to 7 minutes
       //DO NOT DELIVER DO NOT DELIVER
        //int64_t UNSTAKELOCKDURATIONSECONDS = 70;
        //DO NOT DELIVER DO NOT DELIVER


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
            uint32_t insertperiod = (present_time - lockiter->timestamp) + UNSTAKELOCKDURATIONSECONDS;
            if(debugout){
                print (" New lock amount is ",newlockamount,"\n");
                print (" newremaininglockamount ",newremaininglockamount,"\n");
            }
            //the days since launch.
            //DO NOT DELIVER
            uint32_t insertday = (lockiter->timestamp + insertperiod) / SECONDSPERDAY;
            //DO NOT DELIVER
           //TEST ONLY DO NOT DELIVER
           // uint32_t insertday = (lockiter->timestamp + insertperiod) / 10;
            //DO NOT DELIVER
            //if your duration is less than this the period is in the past.
            uint32_t expirednowduration = present_time - lockiter->timestamp;
            uint32_t payouts = lockiter->payouts_performed;
            if(debugout){
                print ("number of payouts performed is ",payouts,"\n");
            }


            vector <eosiosystem::lockperiodv2> newperiods;

            bool insertintoexisting = false;
            uint32_t lastperiodduration = 0;
            int insertindex = -1;
            uint32_t daysforperiod = 0;
            bool foundinsix = false;

            for (int i = 0; i < lockiter->periods.size(); i++) {
                //DO NOT DELIVER
                daysforperiod = (lockiter->timestamp + lockiter->periods[i].duration)/SECONDSPERDAY;
                //DO NOT DELIVER
               //TEST ONLY DO NOT DELIVER
               //daysforperiod = (lockiter->timestamp + lockiter->periods[i].duration)/10;
                //DO NOT DELIVER
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
                if (debugout){
                    print (" adding element at index ",insertindex,"\n");
                }
                if (insertindex == -1) {
                    //insert at the end of the list, my duration is greater than all in the list.
                    newperiods.push_back(iperiod);
                }else {
                        newperiods.insert(newperiods.begin() + insertindex, iperiod);
                }
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
            if(debugout) {
                print(" creating general lock for amount ", lockamount, "\n");
            }
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
