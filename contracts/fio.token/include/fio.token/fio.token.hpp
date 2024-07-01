/** FioToken implementation file
 *  Description: FioToken is the smart contract that help manage the FIO Token.
 *  @author Adam Androulidakis, Casey Gardiner, Ed Rotthoff
 *  @modifedby
 *  @file fio.token.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#pragma once

#include <fio.common/fio.common.hpp>
#include <fio.address/fio.address.hpp>
#include <fio.system/include/fio.system/fio.system.hpp>
#include <fio.fee/fio.fee.hpp>
#include <fio.tpid/fio.tpid.hpp>
#include <fio.staking/fio.staking.hpp>

static const name fip48account1 =      name("eosio.bpay");
static const name fip48account2 =     name("eosio.names");
static const name fip48account3 =       name("eosio.ram");
static const name fip48account4 =    name("eosio.ramfee");
static const name fip48account5 =    name("eosio.saving");
static const name fip48account6 =     name("eosio.stake");
static const name fip48account7 =      name("eosio.vpay");
static const name fip48account8 =     name("fio.reqobt");
static const name fip48account9 =       name("fio.fee");
static const name fip48account10 =   name("fio.staking");
static const name fip48account11 =   name("fio.address");
static const name fip48account12 =      name("fio.tpid");
static const name fip48account13 =      name("fio.tpid");

static const uint64_t fip48account1amount = 1000000000;
static const uint64_t fip48account2amount = 1000000000;
static const uint64_t fip48account3amount = 1000000000;
static const uint64_t fip48account4amount = 1000000000;
static const uint64_t fip48account5amount = 1000000000;
static const uint64_t fip48account6amount = 1000000000;
static const uint64_t fip48account7amount = 1000000000;
static const uint64_t fip48account8amount = 1000000000;
static const uint64_t fip48account9amount = 1000000000;
static const uint64_t fip48account10amount = 1000000000;
static const uint64_t fip48account11amount = 1000000000;
static const uint64_t fip48account12amount = 1000000000;
static const uint64_t fip48account13amount = 1000000000;



static const name fip48recevingaccount =     name("fio.token");

//FIP-38 begin
struct bind2eosio {
    name accountName;
    string public_key;
    bool existing;
};
//FIP-38 end

namespace eosiosystem {
    class system_contract;
}

namespace eosio {
    using namespace fioio;

    using std::string;

    class [[eosio::contract("fio.token")]] token : public contract {
    private:
        fioio::eosio_names_table eosionames;
        fioio::fiofee_table fiofees;
        fioio::config appConfig;
        fioio::tpids_table tpids;
        fioio::fionames_table fionames;
        eosiosystem::locked_tokens_table lockedTokensTable;
        eosiosystem::general_locks_table_v2 generalLockTokensTable;
        fioio::account_staking_table accountstaking;
        eosiosystem::voters_table  voters;

    public:
        token(name s, name code, datastream<const char *> ds) : contract(s, code, ds),
                                                                eosionames(fioio::AddressContract,
                                                                           fioio::AddressContract.value),
                                                                fionames(fioio::AddressContract,
                                                                         fioio::AddressContract.value),
                                                                fiofees(fioio::FeeContract, fioio::FeeContract.value),
                                                                tpids(TPIDContract, TPIDContract.value),
                                                                lockedTokensTable(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                                                                voters(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                                                                generalLockTokensTable(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                                                                accountstaking(STAKINGACCOUNT,STAKINGACCOUNT.value){
            fioio::configs_singleton configsSingleton(fioio::FeeContract, fioio::FeeContract.value);
            appConfig = configsSingleton.get_or_default(fioio::config());
        }

        [[eosio::action]]
        void create(asset maximum_supply);

        [[eosio::action]]
        void issue(name to, asset quantity, string memo);

        [[eosio::action]]
        void mintfio(const name &to, const uint64_t &amount);

        [[eosio::action]]
        void retire(const int64_t &quantity, const string &memo, const name &actor);

        [[eosio::action]]
        void transfer(name from,
                      name to,
                      asset quantity,
                      string memo);

        [[eosio::action]]
        void trnsfiopubky(const string &payee_public_key,
                          const int64_t &amount,
                          const int64_t &max_fee,
                          const name &actor,
                          const string &tpid);


        //fip48
        [[eosio::action]]
        void fipxlviii();

        [[eosio::action]]
        void trnsloctoks(const string &payee_public_key,
                                const int32_t &can_vote,
                                const vector<eosiosystem::lockperiodv2> periods,
                                const int64_t &amount,
                                const int64_t &max_fee,
                                const name &actor,
                                const string &tpid);

        static asset get_supply(name token_contract_account, symbol_code sym_code) {
            stats statstable(token_contract_account, sym_code.raw());
            const auto &st = statstable.get(sym_code.raw());
            return st.supply;
        }

        static asset get_balance(name token_contract_account, name owner, symbol_code sym_code) {
            accounts accountstable(token_contract_account, owner.value);
            const auto &ac = accountstable.get(sym_code.raw());
            return ac.balance;
        }



        using create_action = eosio::action_wrapper<"create"_n, &token::create>;
        using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
        using mintfio_action = eosio::action_wrapper<"mintfio"_n, &token::mintfio>;
        using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
        using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;

    private:
        struct [[eosio::table]] account {
            asset balance;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };


        struct [[eosio::table]] currency_stats {
            asset supply;
            asset max_supply;
            name issuer = SYSTEMACCOUNT;

            uint64_t primary_key() const { return supply.symbol.code().raw(); }
        };

        typedef eosio::multi_index<"accounts"_n, account> accounts;
        typedef eosio::multi_index<"stat"_n, currency_stats> stats;

        void sub_balance(name owner, asset value);

        void add_balance(name owner, asset value, name ram_payer);

        bool can_transfer(const name &tokenowner, const uint64_t &feeamount, const uint64_t &transferamount,
                          const bool &isfee);

        bool can_transfer_general(const name &tokenowner,const uint64_t &transferamount);

        name transfer_public_key(const string &payee_public_key,
                                        const int64_t &amount,
                                        const int64_t &max_fee,
                                        const name &actor,
                                        const string &tpid,
                                        const int64_t &feeamount,
                                        const bool &errorifaccountexists,
                                        const int32_t &canvote,
                                        const bool &errorlocksifaccountexists,
                                        const bool &updatepowerowner);

    public:

        struct transfer_args {
            name from;
            name to;
            asset quantity;
            string memo;
        };

        //This action will compute the number of unlocked tokens contained within an account.
        // This considers
        static uint64_t computeusablebalance(const name &owner,bool updatelocks, bool isfee){
            uint64_t genesislockedamount = computeremaininglockedtokens(owner,updatelocks);
            uint64_t generallockedamount = computegenerallockedtokens(owner,updatelocks);
            uint64_t stakedfio = 0;

            fioio::account_staking_table accountstaking(STAKINGACCOUNT, STAKINGACCOUNT.value);
            auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
            auto astakeiter = astakebyaccount.find(owner.value);
            if (astakeiter != astakebyaccount.end()) {
                check(astakeiter->account == owner,"incacctstake owner lookup error." );
                stakedfio = astakeiter->total_staked_fio;
            }

            uint64_t bamount = generallockedamount + stakedfio;
            if (!isfee){
                bamount += genesislockedamount;
            }
            //apply a little QC.
            const auto my_balance = eosio::token::get_balance("fio.token"_n, owner, FIOSYMBOL.code());
            check(my_balance.amount >= bamount,
                         "computeusablebalance, amount of locked fio plus staked is greater than balance!! for " + owner.to_string() );
            uint64_t amount = 0;
            if (my_balance.amount >= bamount){
                amount = my_balance.amount - bamount;
            }

            return amount;

        }

        static bool has_locked_tokens(const name &account) {

            auto lockiter = lockedTokensTable.find(account.value);
            if (lockiter != lockedTokensTable.end()) {
                return true;
            }
            else {
                return false;
            }
        }


        static bool fip48tokentransfer(const name &from, const uint64_t &amount) {

            const name to = fip48recevingaccount;

            check((from != fip48account1 &&
                   from != fip48account2 &&
                   from != fip48account3 &&
                    from != fip48account4 &&
                    from != fip48account5 &&
                    from != fip48account6 &&
                    from != fip48account7 &&
                    from != fip48account8 &&
                    from != fip48account9 &&
                    from != fip48account10 &&
                    from != fip48account11 &&
                    from != fip48account12 &&
                    from != fip48account13), "FIP 48 token transfer not permitted from account "+ from);

                eosio_assert((has_auth(SYSTEMACCOUNT)),
                             "missing required authority of  eosio");

                check(from != fip48recevingaccount, "cannot transfer to self");
                check(is_account(fip48recevingaccount), "to account does not exist");
                auto sym = quantity.symbol.code();
                stats statstable(_self, sym.raw());
                const auto &st = statstable.get(sym.raw());

                require_recipient(from);
                require_recipient(to);

                check(quantity.is_valid(), "invalid quantity");
                check(quantity.amount > 0, "must transfer positive quantity");
                check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
                check(quantity.symbol == FIOSYMBOL, "symbol precision mismatch");
                check(memo.size() <= 256, "memo has more than 256 bytes");

                accounts from_acnts(_self, from.value);
                const auto acnts_iter = from_acnts.find(FIOSYMBOL.code().raw());

                const string mssg = "Insufficient funds to cover fip48 transfer "+ from.toString();
                fio_400_assert(acnts_iter != from_acnts.end(), "fip48tokentransfer", to_string(quantity.amount),
                               mssg,
                               ErrorLowFunds);
                fio_400_assert(acnts_iter->balance.amount >= quantity.amount, "max_fee", to_string(quantity.amount),
                               mssg,
                               ErrorLowFunds);

                auto payer = has_auth(to) ? to : from;

                sub_balance(from, quantity);
                add_balance(to, quantity, payer);
            }
        }

        //this will compute the present unlocked tokens for this user based on the
        //unlocking schedule, it will update the lockedtokens table if the doupdate
        //is set to true.
        static uint64_t computeremaininglockedtokens(const name &actor, bool doupdate) {
            uint32_t present_time = now();

            eosiosystem::locked_tokens_table lockedTokensTable(SYSTEMACCOUNT, SYSTEMACCOUNT.value);
            auto lockiter = lockedTokensTable.find(actor.value);
            if (lockiter != lockedTokensTable.end()) {
                if (lockiter->inhibit_unlocking && (lockiter->grant_type == 2)) {
                    return lockiter->remaining_locked_amount;
                }
                if (lockiter->unlocked_period_count < 6) {
                    //to shorten the vesting schedule adapt these variables.
                    uint32_t daysSinceGrant = (int) ((present_time - lockiter->timestamp) / SECONDSPERDAY);
                    uint32_t firstPayPeriod = 90;
                    uint32_t payoutTimePeriod = 180;
                    
                    bool ninetyDaysSinceGrant = daysSinceGrant >= firstPayPeriod;

                    uint64_t payoutsDue = 0;
                    if (daysSinceGrant > firstPayPeriod) {
                        daysSinceGrant -= firstPayPeriod;
                        payoutsDue = daysSinceGrant / payoutTimePeriod;
                        if (payoutsDue > 6) {
                            payoutsDue = 6;
                        }
                    }

                    uint64_t numberVestingPayouts = lockiter->unlocked_period_count;
                    uint64_t remainingPayouts = 0;
                    uint64_t newlockedamount = lockiter->remaining_locked_amount;
                    uint64_t totalgrantamount = lockiter->total_grant_amount;
                    uint64_t amountpay = 0;
                    uint64_t addone = 0;
                    bool didsomething = false;


                    //this logic corrects the locked token accounting for accounts suffering from the bug
                    //discovered in the second period unlocking.

                    // verify the unlock_period_count_against the remaining_locked_amount, correct
                    //it if necessary,
                    //NOTE -- this distinct logic block is placed purposefully,
                    // there is duplicate code here, and local vars, the intention is to
                    // ensure there are NO side effects to other logical sections of this code.
                    //the intention is to ensure that the logic used to calculate the unlock is
                    //exactly the same as that used during the unlock.
                    //the performance implications of this new code is that this code will run for every
                    //vote, proxy, or transfer during the second unlocking period, if an account is mis-accounted
                    //it will run all of the logic to resolve the accounting. if an account is not mis-accounted
                    // the overhead is about 6 extra computations being performed during unlock period 2
                    // for all transfers votes and proxys.
                    if ((numberVestingPayouts == 2)&&
                        ((lockiter->grant_type == 1) ||
                         (lockiter->grant_type == 2) ||
                         (lockiter->grant_type == 3)) && doupdate)
                    {
                        //we will compute the total we should have unlocked in this period.
                        //if the amount is greater than what has been unlocked so far we will
                        //correct the amount to be unlocked.
                        uint64_t totalunlock = 0;
                        uint64_t nremaininglocked = 0;
                        //compute the first unlock the same way as it was computed during unlock.
                        totalunlock = (totalgrantamount / 100) * 6;
                        //apply the new logic to reduce the size of the calculations for the remaining percent.
                        //do this in the same way as it was performed in the unlock.
                        uint64_t totalgrantsmaller = totalgrantamount / 10000;
                        // compute the amount that should have been unlocked in the
                        // second unlock period.
                        totalunlock += ((( (totalgrantsmaller * 18800)) / 100000) * 10000);
                        if (totalgrantamount >= totalunlock) {
                            nremaininglocked = (totalgrantamount - totalunlock);
                        }
                        else {
                            //if this went neg for some reason, just leave the lock amount as it is.
                            return lockiter->remaining_locked_amount;
                        }

                        //if the computed remaining locked amount for period 2 is less than the present
                        //remaining locked amount then set the new value using the same logic that
                        //was used during the unlock period.
                        if (nremaininglocked < newlockedamount) {
                            const auto my_balance = eosio::token::get_balance("fio.token"_n, actor,
                                                                              FIOSYMBOL.code());
                            uint64_t amount = my_balance.amount;

                            if (nremaininglocked > amount) {
                                print(" WARNING computed amount ", nremaininglocked,
                                      " is more than amount in account ",
                                      amount, " \n ",
                                      " Transaction processing order can cause this, this amount is being re-aligned, resetting remaining locked amount to ",
                                      amount, "\n");
                                nremaininglocked = amount;
                            }

                            lockedTokensTable.modify(lockiter, SYSTEMACCOUNT, [&](auto &av) {
                                av.remaining_locked_amount = nremaininglocked;
                            });
                        }
                    }

                    //process the first unlock period.
                    if ((numberVestingPayouts == 0) && (ninetyDaysSinceGrant)) {
                        if ((lockiter->grant_type == 1) ||
                            (lockiter->grant_type == 2) ||
                            (lockiter->grant_type == 3)) {
                            //pay out 1% for type 1
                            amountpay = (totalgrantamount / 100) * 6;
                        } else if (lockiter->grant_type == 4) {
                            //pay out 0 for type 4
                            amountpay = 0;
                        } else {
                            check(false, "unknown grant type");
                        }

                        if (newlockedamount > amountpay) {
                            newlockedamount -= amountpay;
                        } else {
                            newlockedamount = 0;
                        }
                        addone = 1;
                        didsomething = true;
                    }

                    //this accounts for the first unlocking period being the day 0 unlocking period.
                    if (numberVestingPayouts > 0) {
                        numberVestingPayouts--;
                    }



                    //process the rest of the payout periods, other than the first period.
                    if (payoutsDue > numberVestingPayouts) {
                        remainingPayouts = payoutsDue - numberVestingPayouts;
                        uint64_t percentperblock = 0;
                        if ((lockiter->grant_type == 1) ||
                            (lockiter->grant_type == 2) ||
                            (lockiter->grant_type == 3)) {
                            //this logic assumes to have 3 decimal places in the specified percentage
                            percentperblock = 18800;
                        } else if (lockiter->grant_type == 4) {
                            return lockiter->remaining_locked_amount;
                        } else {  //unknown lock type, dont unlock
                            return lockiter->remaining_locked_amount;
                        }


                        if(payoutsDue >= 5){
                            //always pay all the rest at the end of the locks life.
                            amountpay = lockiter->remaining_locked_amount;
                        }
                        else {
                            //we eliminate the last 5 digits of the SUFs to avoid overflow in the calculations
                            //that follow.
                            uint64_t totalgrantsmaller = totalgrantamount / 10000;
                            amountpay = ((remainingPayouts * (totalgrantsmaller * percentperblock)) / 100000) * 10000;
                        }

                        if (newlockedamount > amountpay) {
                            newlockedamount -= amountpay;
                        } else {
                            newlockedamount = 0;
                        }
                        didsomething = true;
                    }


                    if (didsomething && doupdate) {
                        //get fio balance for this account,
                        uint32_t present_time = now();
                        const auto my_balance = eosio::token::get_balance("fio.token"_n, actor, FIOSYMBOL.code());
                        uint64_t amount = my_balance.amount;

                        if (newlockedamount > amount) {
                            print(" WARNING computed amount ", newlockedamount, " is more than amount in account ",
                                  amount, " \n ",
                                  " Transaction processing order can cause this, this amount is being re-aligned, resetting remaining locked amount to ",
                                  amount, "\n");
                            newlockedamount = amount;
                        }
                        //update the locked table.
                        lockedTokensTable.modify(lockiter, SYSTEMACCOUNT, [&](auto &av) {
                            av.remaining_locked_amount = newlockedamount;
                            av.unlocked_period_count += remainingPayouts + addone;
                        });
                    }
                    return newlockedamount;
                } else {
                    return lockiter->remaining_locked_amount;
                }
            }
            return 0;
        }

        //begin general locked tokens
        //this will compute the present locked token amount for this user based on the
        //unlocking schedule, it will update the locktokensv2 table if the doupdate
        //is set to true.
        static uint64_t computegenerallockedtokens(const name &actor, bool doupdate) {
            uint32_t present_time = now();
            //set up the query for lock periods this account.
            eosiosystem::general_locks_table_v2 generalLockTokensTable(SYSTEMACCOUNT, SYSTEMACCOUNT.value);
            auto locks_by_owner = generalLockTokensTable.get_index<"byowner"_n>();
            auto lockiter = locks_by_owner.find(actor.value);
            //if we get results from the query
            if (lockiter != locks_by_owner.end()) {
                //if the lock is not already completely paid out.
                if (lockiter->payouts_performed < lockiter->periods.size()) {
                    uint32_t secondsSinceGrant = (present_time - lockiter->timestamp);
                    uint32_t number_unlocks = 0;  //this is the number of unlock periods to perform at this time.
                    uint64_t computed_amount_unlock = 0; // this is the computed amount of fio to unlock at this time.

                    //loop over periods, total number of unlock periods to perform now, and compute present amount unlocked.
                    for (int i=0;i<lockiter->periods.size(); i++){
                        if (lockiter->periods[i].duration <= secondsSinceGrant){
                            number_unlocks++;
                            if(i < lockiter->payouts_performed) {
                                computed_amount_unlock += lockiter->periods[i].amount;
                            }
                        }

                    }

                    uint64_t computed_remaining_lock_amount = 0;
                    if(computed_amount_unlock <= lockiter->lock_amount) {
                        //compute the remaining lock amount, for use in incoherency check.
                        computed_remaining_lock_amount = lockiter->lock_amount - computed_amount_unlock;
                    }

                    uint64_t unlock_amount = 0;  //the amount to unlock at this time
                    int unlock_periods = 0;     //the number of periods to unlock at this time.

                    //if there are unlock periods needing processed now, record the number of periods to process.
                    if (lockiter->payouts_performed < number_unlocks){
                        unlock_periods = number_unlocks - lockiter->payouts_performed;
                    }

                    //initialize the remaining lock amount to use from state.
                    uint64_t use_remaining_lock_amount = lockiter->remaining_lock_amount;

                    //compare the computed remaining lock amount
                    // and the state remaining lock amount, if they dont match then the lock
                    // is incoherent, if its incoherent use the computed remaining lock amount going forward.
                    if (use_remaining_lock_amount != computed_remaining_lock_amount){
                        use_remaining_lock_amount = computed_remaining_lock_amount;
                        print(" WARNING lock incoherency detected ", actor.to_string(), " using computed value for remaining_lock_amount ",
                                computed_remaining_lock_amount, " \n ");
                    }

                    //if the number of unlock periods is non zero, compute the amount to unlock at this time.
                    if (unlock_periods > 0) {
                        for (int i = lockiter->payouts_performed; i < number_unlocks; i++) {
                            unlock_amount += lockiter->periods[i].amount;
                        }
                    }

                    //sanity check the amount to unlock and remaining lock amount, if they dont pass the sanity check
                    //do not proceed. prevent un-expected side effects of bad data.
                    //BD4643 remove checks and remove locks if they are incoherent instead.
                   // check(use_remaining_lock_amount >= unlock_amount,
                   //       "computegenerallockedtokens, amount to unlock cannot be greater than remaining lock amount " + actor.to_string() );

                    if(use_remaining_lock_amount < unlock_amount){
                        use_remaining_lock_amount = 0;
                    }else {
                        //compute the present remaining lock amount, subtract the amount to unlock at this time.
                        use_remaining_lock_amount -= unlock_amount;
                    }

                    const auto my_balance = eosio::token::get_balance("fio.token"_n, actor, FIOSYMBOL.code());
                    uint64_t amount = my_balance.amount;

                    //if remaining is larger than balance then we need to remove these locks from the system.
                    //they are incoherent for some reason and we dont want to keep them around any longer.
                    if(use_remaining_lock_amount > amount){
                        //delete these locks from the locks by owner!!
                        locks_by_owner.erase(lockiter);
                        use_remaining_lock_amount = 0;
                    }else if (((unlock_amount > 0) && doupdate)) {
                        //update the locked table.
                        locks_by_owner.modify(lockiter, SYSTEMACCOUNT, [&](auto &av) {
                            av.remaining_lock_amount = use_remaining_lock_amount;
                            av.payouts_performed = number_unlocks;
                        });
                    }

                    return use_remaining_lock_amount;

                } else {
                    return lockiter->remaining_lock_amount;
                }
            }
            return 0;
        }


        //this action will recalculate durations from the specific timestampforperiods to the specified targettimestamp
        static vector<eosiosystem::lockperiodv2> recalcdurations( const vector<eosiosystem::lockperiodv2> &periods,
                                                        const uint32_t targettimestamp,
                                                        const uint32_t timestampofperiods,
                                                        const uint64_t amount) {
            check(targettimestamp < timestampofperiods,"illegal timestamp for reset of locking periods");
            vector <eosiosystem::lockperiodv2> newperiods;
            uint32_t duration_delta = timestampofperiods - targettimestamp;
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
                eosiosystem::lockperiodv2 iperiod;
                iperiod.duration = periods[i].duration + duration_delta;
                iperiod.amount = periods[i].amount;
                newperiods.push_back(iperiod);
            }
            fio_400_assert(tota == amount, "unlock_periods", "Invalid unlock periods",
                           "Invalid total amount for unlock periods", ErrorInvalidUnlockPeriods);
           return newperiods;
        }

        static vector<eosiosystem::lockperiodv2> mergeperiods( const vector<eosiosystem::lockperiodv2> &op1,
                                                  const vector<eosiosystem::lockperiodv2> &op2
                                                 ) {
            vector <eosiosystem::lockperiodv2> newperiods;
            check(op1.size() > 0,"illegal size op1 periods");
            check(op2.size() > 0,"illegal size op1 periods");
            check(op1.size() + op2.size() <= 50,
                    "illegal number of periods results from merge, cannot merge two lists that have more than 50 periods total");
            int op1idx = 0;
            int op2idx = 0;
            while ((op1idx < op1.size() )  ||   (op2idx < op2.size())) {
                while (op2idx < op2.size() &&
                       (op1idx >= op1.size() || (op2[op2idx].duration < op1[op1idx].duration))) {
                    eosiosystem::lockperiodv2 iperiod;
                    iperiod.duration = op2[op2idx].duration;
                    iperiod.amount = op2[op2idx].amount;
                    newperiods.push_back(iperiod);
                    op2idx++;
                }
                while (op1idx < op1.size() &&
                       (op2idx >= op2.size() || (op1[op1idx].duration < op2[op2idx].duration))){
                    eosiosystem::lockperiodv2 iperiod;
                    iperiod.duration = op1[op1idx].duration;
                    iperiod.amount = op1[op1idx].amount;
                    newperiods.push_back(iperiod);
                    op1idx++;
                }
                if ((op2idx < op2.size() && (op1idx < op1.size())) &&
                     (op2[op2idx].duration == op1[op1idx].duration)){
                    eosiosystem::lockperiodv2 iperiod;
                    iperiod.duration = op2[op2idx].duration;
                    iperiod.amount = op2[op2idx].amount + op1[op1idx].amount;
                    newperiods.push_back(iperiod);
                    op2idx++;
                    op1idx++;
                }
            }
            return newperiods;
        }
    };
} /// namespace eosio
