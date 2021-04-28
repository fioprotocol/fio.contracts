/** Fio Staking implementation file
 *  Description:
 *  @author  Ed Rotthoff
 *  @modifedby
 *  @file fio.staking.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#include "fio.staking.hpp"

namespace fioio {

class [[eosio::contract("Staking")]]  Staking: public eosio::contract {

private:

        global_staking_singleton  staking;
        global_staking_state gstaking;
        account_staking_table accountstaking;
        bool debugout = false;

public:
        using contract::contract;

        Staking(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                staking(_self, _self.value),
                accountstaking(_self,_self.value){
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


  //  (implement 1)
    //incgstake  performs the staking state increments when staking occurs
    //  params
    //     fiostakedsufs, this is the amount of fio staked in SUFs
    //     srpcountsus, this is the number of SRPs computed for this stake, units SUSs
    // logic
    //     increment the combined_token_pool by fiostaked.
    //     increment the staked_token_pool by fiostaked.
    //     increment the global_srp_count by srpcount.
    //
    [[eosio::action]]
    void incgstake(const int64_t &fiostakedsufs, const int64_t &srpcountsus) {
            //check auth fio.staking or fio.system
            eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(STAKINGACCOUNT)),
                   "missing required authority of staking or eosio");
            //     increment the combined_token_pool by fiostaked.
            gstaking.combined_token_pool += fiostakedsufs;
            //     increment the staked_token_pool by fiostaked.
            gstaking.staked_token_pool += fiostakedsufs;
            //     increment the global_srp_count by srpcount.
            gstaking.global_srp_count += srpcountsus;
     }

      //(implement 2)
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

          eosio_assert(gstaking.combined_token_pool >= (fiostakedsufs+fiorewardededsufs),"decgstake combined token pool must be greater or equal to staked sufs plus fio rewarded sufs. " );
          eosio_assert(gstaking.staked_token_pool >= fiostakedsufs,"decgstake staked token pool must be greater or equal to staked sufs. " );
          eosio_assert(gstaking.global_srp_count >= srpcountsus,"decgstake global srp count must be greater or equal to srp count. " );


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

    //(implement 3)
    //FIP-21 actions to update accountstake table.
    //incacctstake  this performs the incrementing of account wise info upon staking.
    // params
    //     owner, this is the account that owns the fio being staked (the signer of the stake)
    //     fiostakesufs, this is the amount of fio being staked, units SUFs
    //     srpawardedsus, this is the number of SRPs being awarded this stake, units SUSs
    // logic
    [[eosio::action]]
    void incacctstake(const name &owner, const int64_t &fiostakedsufs, const int64_t &srpaawardedsus) {
        //check auth fio.staking or fio.system
        eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(STAKINGACCOUNT)),
                     "missing required authority of staking or eosio");
        eosio_assert(fiostakedsufs > 0,"incacctstake fiostakedsuf must be greater than 0. " );
        eosio_assert(srpaawardedsus > 0,"srpaawardedsus fiostakedsuf must be greater than 0. " );

        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto astakeiter = astakebyaccount.find(owner.value);
        if (astakeiter != astakebyaccount.end()) {
            eosio_assert(astakeiter->account == owner,"incacctstake owner lookup error." );
            //update the existing record
            astakebyaccount.modify(astakeiter, _self, [&](struct account_staking_info &a) {
                a.total_staked_fio += fiostakedsufs;
                a.total_srp += srpaawardedsus;
            });
        } else {
            const uint64_t id = accountstaking.available_primary_key();
            accountstaking.emplace(get_self(), [&](struct account_staking_info &p) {
                p.id = id;
                p.account = owner;
                p.total_staked_fio = fiostakedsufs;
                p.total_srp = srpaawardedsus;
            });
        }
    }

    //(implement 4)
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
        eosio_assert(fiostakedsufs > 0,"incacctstake fiostakedsuf must be greater than 0. " );
        eosio_assert(srprewardedsus > 0,"srprewardedsus fiostakedsuf must be greater than 0. " );

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
    void stakefio(const int64_t &amount, const int64_t &max_fee,
                         const string &tpid, const name &actor) {
        require_auth(actor);
        print("EDEDEDEDEDEDEDEDEDEDEDEDED call into stakefio amount ", amount," max_fee ",max_fee," tpid ",tpid," actor ",actor, "\n");

        /*
         * Request is validated per Exception handling.
         *
           stake_fio_tokens fee is collected.

           RAM of signer is increased. amount of RAM increase will be computed during development and updated in this FIP

           SRPs to Award are computed: amount / Rate of Exchange

           Account Tokens Staked is incremented by amount in account related table. Account Tokens Staked cannot be spent by the user.
           Account Staking Reward Point is incremented by SRPs to Award in account related table
          (implement) (call incacctstake)

           Combined Token Pool count is incremented by amount.
           Global SRP count is incremented by SRPs to Award.
           staked_token_pool is incremented by amount.
          (implement) (call incgstake)


           check for maximum FIO transaction size is applied

           Account not voted or proxied	Staker's account has not voted in the last 30 days and is not proxying.	400	"actor"	Value sent in, e.g. "aftyershcu22"	"Account has not voted in the last 30 days and is not proxying."
           Invalid amount value	amount format is not valid	400	"amount"	Value sent in, e.g. "-100"	"Invalid amount value"
           Invalid fee value	max_fee format is not valid	400	"max_fee"	Value sent in, e.g. "-100"	"Invalid fee value"
           Fee exceeds maximum	Actual fee is greater than supplied max_fee	400	"max_fee"	Value sent in, e.g. "1000000000"	"Fee exceeds supplied maximum"
           Insufficient balance	Available (unlocked and unstaked) balance in Staker's account is less than chain fee + amount	400	"max_oracle_fee"	Value sent in, e.g. "100000000000"	"Insufficient balance"
           Invalid TPID	tpid format is not valid	400	"tpid"	Value sent in, e.g. "notvalidfioaddress"	"TPID must be empty or valid FIO address"
           Signer not actor	Signer not actor	403			Type: invalid_signature
         */

        check(is_account(actor),"account must pre exist");
        check(amount > 0,"cannot stake token amount less or equal 0.");
    }

    [[eosio::action]]
    void unstakefio(const int64_t &amount, const int64_t &max_fee,
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

EOSIO_DISPATCH(Staking, (stakefio)(unstakefio)(incgstake)(decgstake)(incacctstake)(decacctstake) )
}
