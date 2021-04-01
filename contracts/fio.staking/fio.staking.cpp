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
        bool debugout = false;

public:
        using contract::contract;

        Staking(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                staking(_self, _self.value){
            gstaking = staking.exists() ? staking.get() : global_staking_state{};
        }


    //FIP-21 actions to update staking state.

    //perfstake performs updates to state required upon staking.
    // params
    //     owner,
    //     fiostakedsufs,
    //     srpsawardedsuss
    // logic
    //   call incgstake
    //   call incacctstake

    //perfunstake performs updates to state required upon unstaking.
    // params
    //     owner,
    //     fiostakedsufs,
    //     srpsawardedsuss
    // logic
    //   call decgstake
    //   call decacctstake


    //incgstake  performs the staking state increments when staking occurs
    //  params
    //     fiostakedsufs, this is the amount of fio staked in SUFs
    //     srpcountsus, this is the number of SRPs computed for this stake, units SUSs
    // logic
    //     increment the combined_token_pool by fiostaked.
    //     increment the staked_token_pool by fiostaked.
    //     increment the global_srp_count by srpcount.

    //decgstake performs the staking state decrements when unstaking occurs
    // params
    //   fiounstakedsufs,  this is the amount of FIO being unstaked, units SUFs
    //   fiorewardedsufs,  this is the amount of FIO being rewarded for this unstaked, units SUFs
    //   srpcountsus,      this is the number of SRPs being rewarded for this unstake, units SUSs
    // logic
    //     decrement the combined_token_pool by fiostaked+fiorewarded.
    //     decrement the staked_token_pool by fiostaked.
    //     decrement the global_srp_count by srpcount.

    //incgrewards performs the staking state increments when rewards are identified during fee collection.
    //  params
    //      fioamountsufs, this is the amount of FIO being added to the rewards (from fees or when minted). units SUFs
    //  logic
    //     increment rewards_token_pool
    //     increment daily_staking_rewards
    //     increment combined_token_pool.

    //clrgdailyrew performs the clearing of the daily rewards.
    // params none!
    // logic
    //   set daily_staking_rewards = 0;

    //incgstkmint increments the staking_rewards_reserves_minted
    // params
    //     amountfiosufs, this is the amount of FIO that has been minted, units SUFs
    //FIP-21 actions to update staking state.

    //FIP-21 actions to update accountstake table.
    //incacctstake  this performs the incrementing of account wise info upon staking.
    // params
    //     owner, this is the account that owns the fio being staked (the signer of the stake)
    //     fiostakesufs, this is the amount of fio being staked, units SUFs
    //     srpawardedsus, this is the number of SRPs being awarded this stake, units SUSs
    // logic
    //decacctstake  this performs the decrementing of account wise info upon staking.
    // params
    //     owner,         this is the account that owns the fio being unstaked (the signer of the unstake)
    //     fiostakesufs,  this is the amount of FIO being unstaked, units SUFs
    //     srprewardedsus, this is the number of SRPs being rewarded this unstake, units SUSs
    // logic
    //FIP-21 actions to update accountstake table.


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
           (call incacctstake)

           Combined Token Pool count is incremented by amount.
           Global SRP count is incremented by SRPs to Award.
           staked_token_pool is incremented by amount.
           (call incgstake)


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
           (call decacctstake)

           Staking Reward Amount is transferred to Staker's Account.
           Memo: "Paying Staking Rewards"

           Global SRP count is decremented by SRPs to Claim .
           Combined Token Pool count is decremented by amount + Staking Reward Amount.
           staked token pool is decremented by amount
           (call decgstake)

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

EOSIO_DISPATCH(Staking, (stakefio)(unstakefio)  )
}
