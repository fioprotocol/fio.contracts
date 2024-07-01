/** FioToken implementation file
 *  Description: FioToken is the smart contract that help manage the FIO Token.
 *  @author Adam Androulidakis, Casey Gardiner, Ed Rotthoff
 *  @modifedby
 *  @file fio.token.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#define MAXFIOMINT 100000000000000000

#include "fio.token/fio.token.hpp"

using namespace fioio;

namespace eosio {

    void token::create(asset maximum_supply) {
        require_auth(_self);

        const auto sym = maximum_supply.symbol;
        check(sym.is_valid(), "invalid symbol name");
        check(maximum_supply.is_valid(), "invalid supply");
        check(maximum_supply.amount > 0, "max-supply must be positive");
        check(maximum_supply.symbol == FIOSYMBOL, "symbol precision mismatch");

        stats statstable(_self, sym.code().raw());
        check(statstable.find(sym.code().raw()) == statstable.end(), "token with symbol already exists");

        statstable.emplace(get_self(), [&](auto &s) {
            s.supply.symbol = maximum_supply.symbol;
            s.max_supply = maximum_supply;
        });
    }

    void token::issue(name to, asset quantity, string memo) {
        const auto sym = quantity.symbol;
        check(sym.is_valid(), "invalid symbol name");
        check(memo.size() <= 256, "memo has more than 256 bytes");
        check(quantity.symbol == FIOSYMBOL, "symbol precision mismatch");

        stats statstable(_self, sym.code().raw());
        auto existing = statstable.find(sym.code().raw());
        check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
        const auto &st = *existing;

        require_auth(FIOISSUER);
        check(quantity.is_valid(), "invalid quantity");
        check(quantity.amount > 0, "must issue positive quantity");

        check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
        check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

        statstable.modify(st, same_payer, [&](auto &s) {
            s.supply += quantity;
        });

        add_balance(FIOISSUER, quantity, FIOISSUER);

        if (to != FIOISSUER) {
            transfer(FIOISSUER, to, quantity, memo);
        }
    }


    void token::mintfio(const name &to, const uint64_t &amount) {
        //can only be called by fio.treasury@active
        require_auth(TREASURYACCOUNT);

        check((to == TREASURYACCOUNT || to == FOUNDATIONACCOUNT),
              "mint fio can only transfer to foundation or treasury accounts.");


        if (amount > 0 && amount < MAXFIOMINT) {
            action(permission_level{"eosio"_n, "active"_n},
                   "fio.token"_n, "issue"_n,
                   make_tuple(to, asset(amount, FIOSYMBOL),
                              string("New tokens produced from reserves"))
            ).send();
        }
    }

    void token::retire(const int64_t &quantity, const string &memo, const name &actor) {
        require_auth(actor);
        fio_400_assert(memo.size() <= 256, "memo", memo, "memo has more than 256 bytes", ErrorInvalidMemo);
        fio_400_assert(quantity >= MINIMUMRETIRE,"quantity", std::to_string(quantity), "Minimum 1000 FIO has to be retired", ErrorRetireQuantity);
        stats statstable(_self, FIOSYMBOL.code().raw());
        auto existing = statstable.find(FIOSYMBOL.code().raw());
        const auto &st = *existing;

        const asset my_balance = eosio::token::get_balance("fio.token"_n, actor, FIOSYMBOL.code());

        fio_400_assert(quantity <= my_balance.amount, "quantity", to_string(quantity),
                       "Insufficient balance",
                       ErrorInsufficientUnlockedFunds);

        auto astakebyaccount = accountstaking.get_index<"byaccount"_n>();
        auto stakeiter = astakebyaccount.find(actor.value);
        if (stakeiter != astakebyaccount.end()) {
          fio_400_assert(stakeiter->total_staked_fio == 0, "actor", actor.to_string(), "Account staking cannot retire", ErrorRetireQuantity); //signature error if user has stake
        }

        auto genlocks = generalLockTokensTable.get_index<"byowner"_n>();
        auto genlockiter = genlocks.find(actor.value);

        if (genlockiter != genlocks.end()) {
          fio_400_assert(genlockiter->remaining_lock_amount == 0, "actor", actor.to_string(), "Account with partially locked balance cannot retire", ErrorRetireQuantity);  //signature error if user has general lock
        }

        auto lockiter = lockedTokensTable.find(actor.value);
        if (lockiter != lockedTokensTable.end()) {
          if (lockiter->remaining_locked_amount > 0) {

            uint64_t unlocked = quantity;
            if(quantity > lockiter->remaining_locked_amount) {
              unlocked  = lockiter->remaining_locked_amount;
            }
            uint64_t new_remaining_unlocked_amount = lockiter->remaining_locked_amount - unlocked;

            INLINE_ACTION_SENDER(eosiosystem::system_contract, updlocked)
                    ("eosio"_n, {{_self, "active"_n}},
                     {actor, new_remaining_unlocked_amount}
                    );

          }
        }

        sub_balance(actor, asset(quantity, FIOSYMBOL));
        statstable.modify(st, same_payer, [&](auto &s) {
          s.supply.amount -= quantity;
        });

        INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
            ("eosio"_n, {{_self, "active"_n}},
              {actor, true}
            );

        const string response_string = string("{\"status\": \"OK\"}");

        send_response(response_string.c_str());

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
          "Transaction is too large", ErrorTransactionTooLarge);

    }

    bool token::can_transfer(const name &tokenowner, const uint64_t &feeamount, const uint64_t &transferamount,
                             const bool &isfee) {

        //get fio balance for this account,
        uint32_t present_time = now();
        const auto my_balance = eosio::token::get_balance("fio.token"_n, tokenowner, FIOSYMBOL.code());
        uint64_t amount = my_balance.amount;

        //see if the user is in the lockedtokens table, if so recompute the balance
        //based on grant type.
        auto lockiter = lockedTokensTable.find(tokenowner.value);
        if (lockiter != lockedTokensTable.end()) {

            uint32_t issueplus210 = lockiter->timestamp + (210 * SECONDSPERDAY);
            if (
                //if lock type 1 or 2 or 3, 4 and not a fee subtract remaining locked amount from balance
                    (((lockiter->grant_type == 1) || (lockiter->grant_type == 2) || (lockiter->grant_type == 3) ||
                      (lockiter->grant_type == 4)) && !isfee) ||
                    //if lock type 2 and more than 210 days since grant and inhibit locking is set then subtract remaining locked amount from balance .
                    //this keeps the type 2 grant from being used for fees if the inhibit locking is not flipped after 210 days.
                    ((lockiter->grant_type == 2) && ((present_time > issueplus210) && lockiter->inhibit_unlocking))
                    ) {
                //recompute the remaining locked amount based on vesting.
                uint64_t lockedTokenAmount = computeremaininglockedtokens(tokenowner, false);//-feeamount;

                //subtract the lock amount from the balance
                if (lockedTokenAmount < amount) {
                    amount -= lockedTokenAmount;
                    return (amount >= transferamount);
                } else {
                    return false;
                }

            } else if (isfee) {

                uint64_t unlockedbalance = 0;
                if (amount > lockiter->remaining_locked_amount) {
                    unlockedbalance = amount - lockiter->remaining_locked_amount;
                }
                if (unlockedbalance >= transferamount) {
                    return true;
                } else {
                    uint64_t new_remaining_unlocked_amount =
                            lockiter->remaining_locked_amount - (transferamount - unlockedbalance);
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updlocked)
                            ("eosio"_n, {{_self, "active"_n}},
                             {tokenowner, new_remaining_unlocked_amount}
                            );
                    return true;
                }
            }
        } else {
            return true;
        }
        //avoid compile warning.
        return true;

    }

    bool token::can_transfer_general(const name &tokenowner, const uint64_t &transferamount) {
        //get fio balance for this account,
        uint32_t present_time = now();
        const auto my_balance = eosio::token::get_balance("fio.token"_n, tokenowner, FIOSYMBOL.code());

        uint64_t amount = my_balance.amount;

        //recompute the remaining locked amount based on vesting.
        uint64_t lockedTokenAmount = computegenerallockedtokens(tokenowner, false);
        //subtract the lock amount from the balance
        if (lockedTokenAmount < amount) {
            amount -= lockedTokenAmount;
            return (amount >= transferamount);
        } else {
            return false;
        }
    }

    name token::transfer_public_key(const string &payee_public_key,
                             const int64_t &amount,
                             const int64_t &max_fee,
                             const name &actor,
                             const string &tpid,
                             const int64_t &feeamount,
                             const bool &errorifaccountexists,
                             const int32_t &canvote,
                             const bool &errorlocksifaccountexists,
                             const bool &updatepowerowner)
                             {

        require_auth(actor);
        asset qty;
        
        fio_400_assert(isPubKeyValid(payee_public_key), "payee_public_key", payee_public_key,
                       "Invalid FIO Public Key", ErrorPubKeyValid);

        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                       "TPID must be empty or valid FIO address",
                       ErrorPubKeyValid);

        qty.amount = amount;
        qty.symbol = FIOSYMBOL;

        fio_400_assert(amount > 0 && qty.amount > 0, "amount", std::to_string(amount),
                       "Invalid amount value", ErrorInvalidAmount);

        fio_400_assert(qty.is_valid(), "amount", std::to_string(amount), "Invalid amount value", ErrorLowFunds);

        fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value.",
                       ErrorMaxFeeInvalid);

        uint128_t endpoint_hash = fioio::string_to_uint128_hash(TRANSFER_TOKENS_PUBKEY_ENDPOINT);

        auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);

        fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", TRANSFER_TOKENS_PUBKEY_ENDPOINT,
                       "FIO fee not found for endpoint", ErrorNoEndpoint);

        uint64_t reg_amount = fee_iter->suf_amount;
        uint64_t fee_type = fee_iter->type;

        fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                       "transfer_tokens_pub_key unexpected fee type for endpoint transfer_tokens_pub_key, expected 0",
                       ErrorNoEndpoint);

        fio_400_assert(max_fee >= reg_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                       ErrorMaxFeeExceeded);


        string payee_account;
        fioio::key_to_account(payee_public_key, payee_account);

        name new_account_name = name(payee_account.c_str());
        const bool accountExists = is_account(new_account_name);

        if (errorifaccountexists){
            fio_400_assert(!(accountExists), "payee_public_key", payee_public_key,
                           "Locked tokens can only be transferred to new account",
                           ErrorPubKeyValid);
        }

        auto other = eosionames.find(new_account_name.value);

        if (other == eosionames.end()) { //the name is not in the table.
            fio_400_assert(!accountExists, "payee_account", payee_account,
                           "Account exists on FIO chain but is not bound in eosionames",
                           ErrorPubAddressExist);

            const auto owner_pubkey = abieos::string_to_public_key(payee_public_key);

            eosiosystem::key_weight pubkey_weight = {
                    .key = owner_pubkey,
                    .weight = 1,
            };

            const auto owner_auth = authority{1, {pubkey_weight}, {}, {}};

            INLINE_ACTION_SENDER(call::eosio, newaccount)
                    ("eosio"_n, {{_self, "active"_n}},
                     {_self, new_account_name, owner_auth, owner_auth}
                    );

            action{
                    permission_level{_self, "active"_n},
                    AddressContract,
                    "bind2eosio"_n,
                    bind2eosio{
                            .accountName = new_account_name,
                            .public_key = payee_public_key,
                            .existing = accountExists
                    }
            }.send();

        } else {
            fio_400_assert(accountExists, "payee_account", payee_account,
                           "Account does not exist on FIO chain but is bound in eosionames",
                           ErrorPubAddressExist);

            eosio_assert_message_code(payee_public_key == other->clientkey, "FIO account already bound",
                                      fioio::ErrorPubAddressExist);

        }

         if(errorlocksifaccountexists){
             if (accountExists) {
                 auto locks_by_owner = generalLockTokensTable.get_index<"byowner"_n>();
                 auto lockiter = locks_by_owner.find(new_account_name.value);
                 if (lockiter != locks_by_owner.end()) {
                     int64_t newlockamount = lockiter->lock_amount + amount;
                     int64_t newremaininglockamount = lockiter->remaining_lock_amount + amount;
                     uint32_t payouts = lockiter->payouts_performed;
                     bool err1 = (canvote == 0) && canvote != lockiter->can_vote;
                     bool err2 = (canvote == 1) && canvote != lockiter->can_vote;
                     string errmsg = "can_vote:0 locked tokens cannot be transferred to an account that contains can_vote:1 locked tokens";
                     if (err2) {
                         errmsg = "can_vote:1 locked tokens cannot be transferred to an account that contains can_vote:0 locked tokens";
                     }
                     fio_400_assert((!err1 && !err2), "can_vote", to_string(canvote),
                                    errmsg, ErrorInvalidValue);
                 }
                 else {
                     fio_400_assert((canvote == 1), "can_vote", to_string(canvote),
                                    "can_vote:0 locked tokens cannot be transferred to an account that already exists",
                                    ErrorInvalidValue);

                 }
             }
         }



                                 fio_fees(actor, asset{(int64_t) reg_amount, FIOSYMBOL}, TRANSFER_TOKENS_PUBKEY_ENDPOINT);
        process_rewards(tpid, reg_amount,get_self(), actor);

        require_recipient(actor);

        if (accountExists) {
            require_recipient(new_account_name);
        }

        INLINE_ACTION_SENDER(eosiosystem::system_contract, unlocktokens)
                ("eosio"_n, {{_self, "active"_n}},
                 {actor}
                );

         //update token locks on receive of tokens
         if(updatepowerowner) {
             INLINE_ACTION_SENDER(eosiosystem::system_contract, unlocktokens)
                     ("eosio"_n, {{_self, "active"_n}},
                      {new_account_name}
                     );
         }
        accounts from_acnts(_self, actor.value);
        const auto acnts_iter = from_acnts.find(FIOSYMBOL.code().raw());
        fio_400_assert(acnts_iter != from_acnts.end(), "amount", to_string(qty.amount),
                       "Insufficient balance",
                       ErrorLowFunds);
        fio_400_assert(acnts_iter->balance.amount >= qty.amount, "amount", to_string(qty.amount),
                       "Insufficient balance",
                       ErrorLowFunds);

        //must do these three in this order!! can transfer can transfer computeusablebalance
        fio_400_assert(can_transfer(actor, feeamount, qty.amount, false), "amount", to_string(qty.amount),
                       "Insufficient balance tokens locked",
                       ErrorInsufficientUnlockedFunds);

        fio_400_assert(can_transfer_general(actor, qty.amount), "actor", to_string(actor.value),
                       "Funds locked",
                       ErrorInsufficientUnlockedFunds);


        uint64_t uamount = computeusablebalance(actor,false,false);
        fio_400_assert(uamount >= qty.amount, "actor", to_string(actor.value),
                       "Insufficient Funds.",
                       ErrorInsufficientUnlockedFunds);


        sub_balance(actor, qty);
        add_balance(new_account_name, qty, actor);

        INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                ("eosio"_n, {{_self, "active"_n}},
                 {actor, true}
                );

        if (accountExists && updatepowerowner) {
            INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                    ("eosio"_n, {{_self, "active"_n}},
                     {new_account_name, true}
                    );
        }


        //check if accounts voted, reset the audit if they have


         //read voters check if sender or receiver is in the voters table, if so reset the audit
         auto votersbyowner = voters.get_index<"byowner"_n>();
         auto votersbyowner_iter = votersbyowner.find(actor.value);
         bool perfreset = false;
         if (votersbyowner_iter != votersbyowner.end()){
             perfreset = true;
         }
         else if (accountExists) {
             votersbyowner_iter = votersbyowner.find(new_account_name.value);
             if (votersbyowner_iter != votersbyowner.end()) {
                 perfreset = true;
             }
         }

          if(perfreset) {
              action(
                      permission_level{get_self(), "active"_n},
                      SYSTEMACCOUNT,
                      "resetaudit"_n,
                      ""
              ).send();
          }

         return new_account_name;
    }





    void token::transfer(name from,
                         name to,
                         asset quantity,
                         string memo) {

        /* we permit the use of transfer from the system account to any other accounts,
         * we permit the use of transfer from the treasury account to any other accounts.
         * we permit the use of transfer from any other accounts to the treasury account for fees.
         */
        if (from != SYSTEMACCOUNT && from != TREASURYACCOUNT && from != EscrowContract
        && from != FIOORACLEContract) {
            if(!has_auth(EscrowContract) && !has_auth(FIOORACLEContract)){
                check(to == TREASURYACCOUNT, "transfer not allowed");
            }
        }
        eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(TREASURYACCOUNT) || has_auth(EscrowContract) || has_auth(FIOORACLEContract)),
                     "missing required authority of treasury or eosio");

        check(from != to, "cannot transfer to self");
        check(is_account(to), "to account does not exist");
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

        fio_400_assert(acnts_iter != from_acnts.end(), "max_fee", to_string(quantity.amount),
                       "Insufficient funds to cover fee",
                       ErrorLowFunds);
        fio_400_assert(acnts_iter->balance.amount >= quantity.amount, "max_fee", to_string(quantity.amount),
                       "Insufficient funds to cover fee",
                       ErrorLowFunds);

        //we need to check the from, check for locked amount remaining
        fio_400_assert(can_transfer(from, 0, quantity.amount, true), "actor", to_string(from.value),
                       "Funds locked",
                       ErrorInsufficientUnlockedFunds);

        fio_400_assert(can_transfer_general(from, quantity.amount), "actor", to_string(from.value),
                       "Funds locked",
                       ErrorInsufficientUnlockedFunds);


        int64_t amount = computeusablebalance(from,false,true);
        fio_400_assert(amount >= quantity.amount, "actor", to_string(from.value),
                       "Insufficient Funds.",
                       ErrorInsufficientUnlockedFunds);

        //read voters check if sender or receiver is in the voters table, if so reset the audit
        auto votersbyowner = voters.get_index<"byowner"_n>();
        auto votersbyowner_iter = votersbyowner.find(to.value);
        bool perfreset = false;
        if (votersbyowner_iter != votersbyowner.end()){
            perfreset = true;
        }
        else {
            votersbyowner_iter = votersbyowner.find(from.value);
            if (votersbyowner_iter != votersbyowner.end()) {
                perfreset = true;
            }
        }

        if(perfreset) {
            action(
                    permission_level{get_self(), "active"_n},
                    SYSTEMACCOUNT,
                    "resetaudit"_n,
                    ""
            ).send();
        }

        auto payer = has_auth(to) ? to : from;

        sub_balance(from, quantity);
        add_balance(to, quantity, payer);
    }

    void token::trnsfiopubky(const string &payee_public_key,
                             const int64_t &amount,
                             const int64_t &max_fee,
                             const name &actor,
                             const string &tpid) {

       uint128_t endpoint_hash = fioio::string_to_uint128_hash("transfer_tokens_pub_key");

       auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
       auto fee_iter = fees_by_endpoint.find(endpoint_hash);

       fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", "transfer_tokens_pub_key",
                      "FIO fee not found for endpoint", ErrorNoEndpoint);

       uint64_t reg_amount = fee_iter->suf_amount;
       uint64_t fee_type = fee_iter->type;

       fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                      "transfer_tokens_pub_key unexpected fee type for endpoint transfer_tokens_pub_key, expected 0",
                      ErrorNoEndpoint);

       fio_400_assert(max_fee >= reg_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                      ErrorMaxFeeExceeded);

        //do the transfer
        transfer_public_key(payee_public_key,amount,max_fee,actor,tpid,reg_amount,false,0,false,true);

        if (TRANSFERPUBKEYRAM > 0) {
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, TRANSFERPUBKEYRAM)
            ).send();
        }



        const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                       to_string(reg_amount) + string("}");

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
          "Transaction is too large", ErrorTransactionTooLarge);

        send_response(response_string.c_str());

    }

    bool has_locked_tokens(const name &account) {

        auto lockiter = lockedTokensTable.find(account.value);
        if (lockiter != lockedTokensTable.end()) {
            return true;
        }
        else {
            return false;
        }
    }

    //fip48
    void token::fipxlviii(){

        /*
         * these are the accounts for local dev net testing.
         * final account names need to be edited into place as a last step!!
         *
         * Private key: 5HtvSyYQm5aNjuhKzHqDtgkCeHjqbybehhsrdgr9Jt29ERAD5ib
Public key: FIO5p54hi18swMeJMdVD7cdn1kKh76sa8QWhoCumeymHeHFkS9UTy
FIO Public Address (actor name): tdcfarsowlnk
9,999,960

Private key: 5KK3HbWJrD1ejXa7tYxo78WAcq2upVRUSjKrZviGTZT1DsZYCoy
Public key: FIO5hViAQMMTjhkmyqJE3hN98MxKybXe8jFDgRsbt4Q684BgzeBi4
FIO Public Address (actor name): evorvygfnrzk
10,000,000



Private key: 5JwmDtsJDTY2M3h9bsXZDD2tHPj3UgQf7FVpptaLeC7NzxeXnXu
Public key: FIO8WaU8ZT9YLixZZ41uHiYmkoRSZHgCR3anfL3YupC3boQpwvXqG
FIO Public Address (actor name): xbfugtkzvowu
7,000,000



Private key: 5J3u7pZpoLN1zxM8ZDfnaxvTLaJxcyuZ4mWB5V9xgESCC9Wgqck
Public key: FIO5sHPV7sVTNNvMZag7HMJyTWPJVVLgueUuWTfNSbxQwByM9gp9D
FIO Public Address (actor name): p1kv5e2zdxbh
5,500,000


Private key: 5JvnF4B2g34pnexdrTMP7TMcRXz34FUNGhMQG9nR1mzRbX2s5QD
Public key: FIO5ju7xzrDLUwC93ZhHjewWJqb1m2ihcxjiN9N9UDasn5FCpgJT2
FIO Public Address (actor name): kk2gys4vl5ve
2,500,000


Private key: 5KXSKdDrM1yJMTVthXH2aGzhzaoC7HwBdk9ADJhz8jJxGt77PxL
Public key: FIO7vQq9XfSrvDD4uPF4EN2UNE4xXRXMyNmWdtbSKGFktkFAQ2Xuy
FIO Public Address (actor name): jnp3viqz32tc
1,999,999.4



Private key: 5KZPzhRT7g4K2cdiXYf4Jwu6jBgu69FaDYsPKNJ3Xs3A617fkeQ
Public key: FIO5EKfrouMtuS8tY8xZXmhiSHeMJFaPVjG9qCeq2fR4WUXSd2NNf
FIO Public Address (actor name): hcfsdi2vybrv
1,500,000


Private key: 5JrKqjNYw4p65csSXbanj7KdbiAConME66ybjwQx9cUwHX7jUK9
Public key: FIO75t8gA8JPJqGgMAjpvFhkKtK2dPtRdLiyxUG6kTNogXPq1A1bF
FIO Public Address (actor name): 125nkypgqojv
1000


Private key: 5JeBBi58iKkxdwWJBz85vLfcBBC8uRGKaocLR16QoGQQFT8qpNT
Public key: FIO5oQPqujG8qiKkNPuWbdm8NGiYM3STuhHS8bXQ2dgNDaEg1aYNr
FIO Public Address (actor name): sauhngb2eq1c
1000

Private key: 5J5dtsQA8zWpq1QuJXuD564ZHXupGq9y11TDeXLNr6o9xWxykKk
Public key: FIO5EpXzNWv9qDbhgnN3dMcAXVZykSHZswktarqC9G6W2HwEGA26v
FIO Public Address (actor name): idmwqtsmij4i
1000


Private key: 5Jm7GhEMzA3Ck9xougP5hhCPcFa6bBLNFAzCSdff9eevuVy4AGh
Public key: FIO6BEUsLHUGJcC89RQYVQJRbSF8Za5PdiD5bzZiCnpirTSBLaUmy
FIO Public Address (actor name): dq5q2kx5oioa
1000

Private key: 5JKBCzEUSejvayhhrLW88bCn4ReaZekU3wgGLTcW2CDKS1vkGS4
Public key: FIO5vP2CiVzeM2HntW9MPLGG2RWkAxfNyv3DgGL5EDoef6gALb4pR
FIO Public Address (actor name): bxg2u5gpgoc2
1000


Private key: 5J8wgFpv919HmjppHjGsQQuSYqa4AeLxwtAR8a4WaEtZeNin4Ue
Public key: FIO7svM1qskdtW37AbKvuKyrjm182en1xuskh8zcPHozyhfGuqt53
FIO Public Address (actor name): dffmxsxuq1gt
1000
         *
         *
         *
         *
         * receiver account
         *  Private key: 5JGyp6ZDEYHsPfEGrEXQdKNJFSvtvMoBuPNWwpfkUi5vQFsu5PU
Public key: FIO6gPtYH9FzBNSqEfft143Xzt7M5HMW2C3XNSe2aQDAA2csSBP4s
FIO Public Address (actor name): fidgtwmzrrjq
         */








        uint64_t totalamounttransfer = 0;

 //only callable by eosio account.
 eosio_assert(has_auth(SYSTEMACCOUNT),
                             "missing required authority of eosio");

 //reallocate for account1
 eosio_assert(has_locked_tokens(fip48account1), "fip48 NO WORK PERFORMED account has no lockedtokens table entry "+ fip48account1);
 fip48tokentransfer(fip48account1,fip48account1amount);
 action(
        permission_level{get_self(), "active"_n},
        SYSTEMACCOUNT,
        "remgenesis"_n,
        std::make_tuple(fip48account1)
 ).send();

 totalamounttransfer += fip48account1amount;







/*
 fip48modifyreceiveraccountlocks(totaltransferamount);


Remove the lockedtokens entry for this account from the lockedtokens table.
Increment total amount transferred by the amount in Step 2.



 Modify the lockedtokens record for the target account.
Set the total_grant_amount to be the total transferred.
Set the remaining_lock_amount to be the total transferred.
Report status: ok total_transferred_amount : amount transferred
 */









        const string response_string = string("{\"status\": \"OK\",\"total_transferred\":") +
                                       to_string(0) +
                                       string(",\"status_code\":") + to_string(0) +
                                       string("}");
        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                       "Transaction is too large", ErrorTransactionTooLarge);

        send_response(response_string.c_str());
    }

    void token::trnsloctoks(const string &payee_public_key,
                             const int32_t &can_vote,
                             const vector<eosiosystem::lockperiodv2> periods,
                             const int64_t &amount,
                             const int64_t &max_fee,
                             const name &actor,
                             const string &tpid) {

        fio_400_assert(((periods.size()) >= 1 && (periods.size() <= 50)), "unlock_periods", "Invalid unlock periods",
                       "Invalid number of unlock periods", ErrorTransactionTooLarge);

        uint32_t present_time = now();

        fio_400_assert(((can_vote == 0)||(can_vote == 1)), "can_vote", to_string(can_vote),
                       "Invalid can_vote value", ErrorInvalidValue);

        uint128_t endpoint_hash = fioio::string_to_uint128_hash("transfer_locked_tokens");

        auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
        auto fee_iter = fees_by_endpoint.find(endpoint_hash);

        fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", "transfer_locked_tokens",
                       "FIO fee not found for endpoint", ErrorNoEndpoint);

        uint64_t reg_amount = fee_iter->suf_amount;
        uint64_t fee_type = fee_iter->type;

        fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                       "transfer_tokens_pub_key unexpected fee type for endpoint transfer_tokens_pub_key, expected 0",
                       ErrorNoEndpoint);

        fio_400_assert(max_fee >= reg_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                       ErrorMaxFeeExceeded);


        int64_t ninetydayperiods = periods[periods.size()-1].duration / (SECONDSPERDAY * 90);
        int64_t rem = periods[periods.size()-1].duration % (SECONDSPERDAY * 90);
        if (rem > 0){
            ninetydayperiods++;
        }
        reg_amount = ninetydayperiods * reg_amount;

        //check for pre existing account is done here.
        name owner = transfer_public_key(payee_public_key,amount,max_fee,actor,tpid,reg_amount,false,can_vote,true,false);

        //FIP-41 new logic for send lock tokens to existing account
        auto locks_by_owner = generalLockTokensTable.get_index<"byowner"_n>();
        auto lockiter = locks_by_owner.find(owner.value);
        if (lockiter != locks_by_owner.end()) {
            int64_t newlockamount = lockiter->lock_amount + amount;
            int64_t newremaininglockamount = lockiter->remaining_lock_amount + amount;
            uint32_t payouts = lockiter->payouts_performed;
            vector<eosiosystem::lockperiodv2> periods_t1 = recalcdurations(periods,lockiter->timestamp, present_time, amount);
            vector <eosiosystem::lockperiodv2> newperiods = mergeperiods(periods_t1,lockiter->periods);
            action(
                    permission_level{get_self(), "active"_n},
                    SYSTEMACCOUNT,
                    "modgenlocked"_n,
                    std::make_tuple(owner, newperiods, newlockamount, newremaininglockamount, payouts)
            ).send();
        }else {
            uint64_t tota = 0;
            double tv = 0.0;

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
            const bool canvote = (can_vote == 1);

            INLINE_ACTION_SENDER(eosiosystem::system_contract, addgenlocked)
                    ("eosio"_n, {{_self, "active"_n}},
                     {owner, periods, canvote, amount}
                    );
        }
        // end FIP-41 logic for send lock tokens to existing account
        //because we adapt locks do one more voting power calc here.
        INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                    ("eosio"_n, {{_self, "active"_n}},
                     {owner, true}
                    );

        int64_t raminc = 1200;

        action(
                permission_level{SYSTEMACCOUNT, "active"_n},
                "eosio"_n,
                "incram"_n,
                std::make_tuple(actor, raminc)
                ).send();


        const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                       to_string(reg_amount) + string("}");
        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                       "Transaction is too large", ErrorTransactionTooLarge);

        send_response(response_string.c_str());

    }


    void token::sub_balance(name owner, asset value) {
        accounts from_acnts(_self, owner.value);
        const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");

        fio_400_assert(from.balance.amount >= value.amount, "amount", to_string(value.amount),
                       "Insufficient balance", ErrorLowFunds);

        from_acnts.modify(from, owner, [&](auto &a) {
            a.balance -= value;
        });
    }

    void token::add_balance(name owner, asset value, name ram_payer) {
        accounts to_acnts(_self, owner.value);
        auto to = to_acnts.find(value.symbol.code().raw());
        if (to == to_acnts.end()) {
            to_acnts.emplace(ram_payer, [&](auto &a) {
                a.balance = value;
            });
        } else {
            to_acnts.modify(to, same_payer, [&](auto &a) {
                a.balance += value;
            });
        }
    }
} /// namespace eosio

EOSIO_DISPATCH( eosio::token, (create)(issue)(mintfio)(transfer)(trnsfiopubky)(trnsloctoks)(retire)(fipxlviii))
