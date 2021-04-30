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

namespace fioio {

class [[eosio::contract("Staking")]]  Staking: public eosio::contract {

private:

        global_staking_singleton  staking;
        global_staking_state gstaking;
        account_staking_table accountstaking;
        eosiosystem::voters_table voters;
        fionames_table fionames;
        fiofee_table fiofees;
        bool debugout = false;

public:
        using contract::contract;

        Staking(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                staking(_self, _self.value),
                accountstaking(_self,_self.value),
                voters(SYSTEMACCOUNT,SYSTEMACCOUNT.value),
                fiofees(FeeContract, FeeContract.value),
                fionames(AddressContract, AddressContract.value){
            gstaking = staking.exists() ? staking.get() : global_staking_state{};
        }

        ~Staking() {
            staking.set(gstaking, _self);
        }


    //FIP-21 actions to update staking state.

    //(implement 5)
    //perfstake performs updates to state required upon staking.
    // params
    //     owner,
    //     fiostakedsufs,
    //     srpsawardedsuss
    // logic
    //   call incgstake
    //   call incacctstake

    //(implement 6)
    //perfunstake performs updates to state required upon unstaking.
    // params
    //     owner,
    //     fiostakedsufs,
    //     srpsawardedsuss
    // logic
    //   call decgstake
    //   call decacctstake


    //decgstake performs the staking state decrements when unstaking occurs
    // params
    //   fiounstakedsufs,  this is the amount of FIO being unstaked, units SUFs
    //   fiorewardedsufs,  this is the amount of FIO being rewarded for this unstaked, units SUFs
    //   srpcountsus,      this is the number of SRPs being rewarded for this unstake, units SUSs
    // logic
    //     decrement the combined_token_pool by fiostaked+fiorewarded.
    //     decrement the staked_token_pool by fiostaked.
    //     decrement the global_srp_count by srpcount.
      [[eosio::action]]
      void decgstake(const int64_t &fiostakedsufs, const int64_t &fiorewardededsufs,const int64_t &srpcountsus) {
          //check auth fio.staking or fio.system
          eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(STAKINGACCOUNT)),
                       "missing required authority of staking or eosio");

          //avoid overflows due to negative results.
          eosio_assert(gstaking.combined_token_pool >= (fiostakedsufs+fiorewardededsufs),"decgstake combined token pool must be greater or equal to staked sufs plus fio rewarded sufs. " );
          eosio_assert(gstaking.staked_token_pool >= fiostakedsufs,"decgstake staked token pool must be greater or equal to staked sufs. " );
          eosio_assert(gstaking.global_srp_count >= srpcountsus,"decgstake global srp count must be greater or equal to srp count. " );

          //should we compute an intermediate result here and check it against supply.

          //     decrement the combined_token_pool by fiostaked+fiorewarded.
          gstaking.combined_token_pool -= (fiostakedsufs+fiorewardededsufs);
          //     decrement the staked_token_pool by fiostaked.
          gstaking.staked_token_pool -= fiostakedsufs;
          //     decrement the global_srp_count by srpcount.
          gstaking.global_srp_count -= srpcountsus;
      }

    //(implement 7)
    //incgrewards performs the staking state increments when rewards are identified during fee collection.
    //  params
    //      fioamountsufs, this is the amount of FIO being added to the rewards (from fees or when minted). units SUFs
    //  logic
    //     increment rewards_token_pool
    //     increment daily_staking_rewards
    //     increment combined_token_pool.

    //(implement 8)
    //clrgdailyrew performs the clearing of the daily rewards.
    // params none!
    // logic
    //   set daily_staking_rewards = 0;

    //(implement 9)
    //incgstkmint increments the staking_rewards_reserves_minted
    // params
    //     amountfiosufs, this is the amount of FIO that has been minted, units SUFs
    //FIP-21 actions to update staking state.


    //decacctstake  this performs the decrementing of account wise info upon staking.
    // params
    //     owner,         this is the account that owns the fio being unstaked (the signer of the unstake)
    //     fiostakesufs,  this is the amount of FIO being unstaked, units SUFs
    //     srprewardedsus, this is the number of SRPs being rewarded this unstake, units SUSs
    // logic
    //FIP-21 actions to update accountstake table.
    [[eosio::action]]
    void decacctstake(const name &owner, const int64_t &fiostakedsufs, const int64_t &srprewardedsus) {
        //check auth fio.staking or fio.system
        eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(STAKINGACCOUNT)),
                     "missing required authority of staking or eosio");
        eosio_assert(fiostakedsufs > 0,"decacctstake fiostakedsuf must be greater than 0. " );
        eosio_assert(srprewardedsus > 0,"decacctstake srprewardedsus must be greater than 0. " );

        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(owner.value);

        eosio_assert(astakeiter != astakebyaccount.end(),"decacctstake owner not found in account staking." );
        eosio_assert(astakeiter->account == owner,"decacctstake owner lookup error." );
        eosio_assert(astakeiter->total_srp >= srprewardedsus,"decacctstake total srp for account must be greater than or equal srprewardedsus." );
        eosio_assert(astakeiter->total_staked_fio >= fiostakedsufs,"decacctstake total staked fio for account must be greater than or equal fiostakedsufs." );

        //update the existing record
        astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
            a.total_staked_fio -= fiostakedsufs;
            a.total_srp -= srprewardedsus;
        });

    }



    [[eosio::action]]
    void stakefio(const string &fio_address, const int64_t &amount, const int64_t &max_fee,
                         const string &tpid, const name &actor) {
        //signer not actor.
        require_auth(actor);
        print("EDEDEDEDEDEDEDEDEDEDEDEDED call into stakefio amount ", amount," max_fee ",max_fee," tpid ",tpid," actor ",actor, "\n");

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

        //process the fio address specified
        FioAddress fa;
        getFioAddressStruct(fio_address, fa);

        fio_400_assert(validateFioNameFormat(fa) && !fa.domainOnly, "fio_address", fa.fioaddress, "Invalid FIO Address",
                       ErrorDomainAlreadyRegistered);

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

        //get the usable balance for the account
        //this is account balance - genesis locked tokens - general locked balance.
        auto stakeablebalance = eosio::token::computeusablebalance(actor);


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

        const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;

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
            rateofexchange = gstaking.combined_token_pool / gstaking.global_srp_count;
        }

        uint64_t srptoaward = amount / rateofexchange;

        gstaking.combined_token_pool += amount;
        gstaking.global_srp_count += srptoaward;
        gstaking.staked_token_pool += amount;


        //increment account staking info
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
        print("EDEDEDEDEDEDEDEDEDEDEDEDED call into unstakefio amount ", amount," max_fee ",max_fee," tpid ",tpid," actor ",actor, "\n");

        /*
         * Request is validated per Exception handling.
         *
           unstake_fio_tokens fee is collected.

           RAM of signer is increased, amount of ram increment will be computed and updated into FIP during development

           SRPs to Claim are computed: Staker's Account SRPs * (Unstaked amount / Total Tokens Staked in Staker's Account)

           Staking Reward Amount is computed: ((SRPs to Claim * Rate of Exchnage) - Unstake amount) * 0.9

           TPID Reward Amount is computed: ((SRPs to Claim * Rate of Exchnage) - Unstake amount) * 0.1

           Account Tokens Staked is decremented by amount in account related table.
           Account Staking Reward Point is decremented by SRPs to Award in account related table
         (implement)  (call decacctstake)

           Staking Reward Amount is transferred to Staker's Account.
           Memo: "Paying Staking Rewards"

           Global SRP count is decremented by SRPs to Claim .
           Combined Token Pool count is decremented by amount + Staking Reward Amount.
           staked token pool is decremented by amount
          (implement) (call decgstake)

           If tpid was provided, TPID Reward Amount is awarded to the tpid and decremented from Combined Token Pool.
           check for max FIO transaction size exceeded will be applied.

           amount + Staking Reward Amount is locked in Staker's Account for 7 days.

           Invalid amount value	amount format is not valid	400	"amount"	Value sent in, e.g. "-100"	"Invalid amount value"
           Ustake exceeds staked	amount to unstake is greater than the total staked by account	400	"amount"	Value sent in, e.g. "100000000000"	"Cannot unstake more than staked."
           Invalid fee value	max_fee format is not valid	400	"max_fee"	Value sent in, e.g. "-100"	"Invalid fee value"
           Fee exceeds maximum	Actual fee is greater than supplied max_fee	400	"max_fee"	Value sent in, e.g. "1000000000"	"Fee exceeds supplied maximum"
           Insufficient balance	Available (unlocked and unstaked) balance in Staker's account is less than chain fee + amount	400	"max_oracle_fee"	Value sent in, e.g. "100000000000"	"Insufficient balance"
           Invalid TPID	tpid format is not valid	400	"tpid"	Value sent in, e.g. "notvalidfioaddress"	"TPID must be empty or valid FIO address"
           Signer not actor	Signer not actor	403			Type: invalid_signature
         */

        check(is_account(actor),"account must pre exist");
        check(amount > 0,"cannot unstake token amount less or equal 0.");
    }

};     //class Staking

EOSIO_DISPATCH(Staking, (stakefio)(unstakefio)(decgstake)(decacctstake) )
}
