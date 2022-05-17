/** FioAsset implementation file
 *  Description: FioAsset is the FIO Contract for creating and transferring FIO Assets
 *  @author Adam Androulidakis
 *  @modifedby
 *  @file fio.asset.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#include "fio.asset/fio.asset.hpp"

using namespace fioio;

namespace eosio {

    void token::create(asset maximum_supply) {
        require_auth(_self);

        const auto sym = maximum_supply.symbol;
        check(sym.is_valid(), "invalid symbol name");
        check(maximum_supply.is_valid(), "invalid supply");
        check(maximum_supply.amount > 0, "max-supply must be positive");
        check(maximum_supply.symbol == maximum_supply.symbol, "symbol precision mismatch");

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

        stats statstable(_self, sym.code().raw());
        auto existing = statstable.find(sym.code().raw());
        check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
        const auto &st = *existing;

        require_auth(st.issuer);
        check(quantity.is_valid(), "invalid quantity");
        check(quantity.amount > 0, "must issue positive quantity");

        check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
        check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

        statstable.modify(st, same_payer, [&](auto &s) {
            s.supply += quantity;
        });

        add_balance( st.issuer, quantity, st.issuer );

    }


    void token::retire(const asset &quantity, const string &memo, const name &actor) {
        require_auth(actor);
        auto sym = quantity.symbol;
        fio_400_assert(memo.size() <= 256, "memo", memo, "memo has more than 256 bytes", ErrorInvalidMemo);
        stats statstable(_self, sym.code().raw());
        auto existing = statstable.find(sym.code().raw());
        const auto &st = *existing;

        const asset my_balance = eosio::token::get_balance("fio.asset"_n, actor, sym.code());

        fio_400_assert(quantity.amount <= my_balance.amount, "quantity", to_string(quantity.amount),
                       "Insufficient balance",
                       ErrorInsufficientUnlockedFunds);

        sub_balance(actor, quantity);
        statstable.modify(st, same_payer, [&](auto &s) {
          s.supply.amount -= quantity.amount;
        });

        const string response_string = string("{\"status\": \"OK\"}");

        send_response(response_string.c_str());

        fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
          "Transaction is too large", ErrorTransactionTooLarge);

    }

    void token::burn(const asset &quantity, const string &memo, const name &actor) {
      retire(quantity, memo, actor);
    }

    name token::transfer_public_key(const string &payee_public_key,
                             const asset &amount,
                             const int64_t &max_fee,
                             const name &actor,
                             const string &tpid,
                             const int64_t &feeamount,
                             const bool &errorifaccountexists)
                             {

        require_auth(actor);
        asset qty;

        fio_400_assert(isPubKeyValid(payee_public_key), "payee_public_key", payee_public_key,
                       "Invalid FIO Public Key", ErrorPubKeyValid);

        fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                       "TPID must be empty or valid FIO address",
                       ErrorPubKeyValid);

        qty.amount = amount.amount;
        qty.symbol = amount.symbol;

        fio_400_assert(amount.amount > 0 && qty.amount > 0, "amount", std::to_string(amount.amount),
                       "Invalid amount value", ErrorInvalidAmount);

        fio_400_assert(qty.is_valid(), "amount", std::to_string(amount.amount), "Invalid amount value", ErrorLowFunds);

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

        name account_name = name(payee_account.c_str());

        fio_fees(actor, asset{(int64_t) reg_amount, FIOSYMBOL}, TRANSFER_TOKENS_PUBKEY_ENDPOINT);
        process_rewards(tpid, reg_amount,get_self(), actor);

        require_recipient(actor);

        INLINE_ACTION_SENDER(eosiosystem::system_contract, unlocktokens)
                ("eosio"_n, {{_self, "active"_n}},
                 {actor}
                );

        accounts from_acnts(_self, actor.value);
        const auto acnts_iter = from_acnts.find(FIOSYMBOL.code().raw());
        fio_400_assert(acnts_iter != from_acnts.end(), "amount", to_string(qty.amount),
                       "Insufficient balance",
                       ErrorLowFunds);
        fio_400_assert(acnts_iter->balance.amount >= qty.amount, "amount", to_string(qty.amount),
                       "Insufficient balance",
                       ErrorLowFunds);

        sub_balance(actor, qty);
        add_balance(account_name, qty, actor);

        return account_name;
    }

    /*
    void token::transfer(name from,
                         name to,
                         asset quantity,
                         string memo) {

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
        check(memo.size() <= 256, "memo has more than 256 bytes");

        accounts from_acnts(_self, from.value);
        const auto acnts_iter = from_acnts.find(sym.code().raw());

        fio_400_assert(acnts_iter != from_acnts.end(), "max_fee", to_string(quantity.amount),
                       "Insufficient funds to cover fee",
                       ErrorLowFunds);
        fio_400_assert(acnts_iter->balance.amount >= quantity.amount, "max_fee", to_string(quantity.amount),
                       "Insufficient funds to cover fee",
                       ErrorLowFunds);

        auto payer = has_auth(to) ? to : from;

        sub_balance(from, quantity);
        add_balance(to, quantity, payer);
    }
    */

    void token::trnspubky(const string &payee_public_key,
                             const asset &amount,
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
        transfer_public_key(payee_public_key,amount,max_fee,actor,tpid,reg_amount,false);

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

EOSIO_DISPATCH( eosio::token, (create)(issue)(burn)(trnspubky)(retire))
