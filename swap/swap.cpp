#include <string>
#include <algorithm>

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

#include "../methods.hpp"

//using namespace std;
using namespace eosio;

class [[eosio::contract]] swap : public contract {
    public:
        using contract::contract;

        struct [[eosio::table]] token : public _token {
        };
        struct [[eosio::table]] pair : public _pair {
        };
        struct [[eosio::table]] order : public _order {
        };
        struct [[eosio::table]] config : public _config {
        };
        struct [[eosio::table]] swaphistory : public _swaphistory {
        };

        swap(name receiver, name code, datastream<const char*> ds) : 
            contract(receiver, code, ds),
            pairs_table(_self, _self.value),
            orders_table(_self, _self.value),
            config_table(_self, _self.value),
            swaphistorys_table(_self, _self.value)
        {
        }

        [[eosio::on_notify("*::transfer")]] 
        void transfer(name from, name to, asset quantity, std::string memo) {
            if (from == _self) return;
            if(to != _self) return;
            if (is_swap(memo)) {
                do_swap(from, to, quantity, memo);
            } else if (is_deposit(memo)) {
                do_deposit(from, to, quantity, memo); 
            } else if (is_withdraw(memo)) {
                do_withdraw(from, to, quantity, memo);
            } else {
                check (false, "illegal transfer");
            }
        }
        
        [[eosio::action]]
        void createpair(name creator, token token0, token token1) {
            require_auth(_self);
            uint64_t last_id = 0;
            if (pairs_table.begin() != pairs_table.end()) {
                last_id = pairs_table.rbegin()->id;
            }
            uint16_t lptoken_precision = std::max(token0.symbol.precision(), token1.symbol.precision());
            uint64_t current_id = last_id + 1;
            symbol lptoken_symbol = create_lptoken_symbol(current_id, lptoken_precision);
            pairs_table.emplace(creator, [&](auto& row) {
                row.id = current_id;
                row.token0 = token0;
                row.token1 = token1;
                row.reserve0 = asset(0, token0.symbol);
                row.reserve1 = asset(0, token1.symbol);
                row.liquidity_token =  asset(0, lptoken_symbol);
                row.burn_amount = asset(0, symbol("ECH", 4));
                row.mine_amount = asset(0, symbol("ECH", 4));
                row.price0_last = 0;
                row.price1_last = 0;
                row.block_time_last = publication_time();
            });
            action(
                permission_level{_self, "active"_n},
                _self,
                "createlog"_n,
                std::make_tuple(last_id + 1, creator, lptoken_symbol, token0, token1)
            ).send();
            action(
                permission_level{LPTOKEN_CONTRACT, "active"_n},
                LPTOKEN_CONTRACT,
                "create"_n,
                std::make_tuple(_self, asset(LPTOKEN_MAXIMUM_SUPPLY, lptoken_symbol))
            ).send();
        }

        [[eosio::action]]
        void deposit(name owner, uint64_t pair_id, name inviter) {
            require_auth(owner);
            auto pair_idx = pairs_table.find(pair_id);
            check (pair_idx != pairs_table.end(), "illegal pair_id");
            check (orders_table.begin() != orders_table.end(), "illegal deposit");
            auto order_idx = orders_table.find(orders_table.rbegin()->id);
            check ( order_idx != orders_table.end() && 
                    order_idx->owner == owner && 
                    order_idx->pair_id == pair_id &&
                    order_idx->token0.amount > 0 &&
                    order_idx->token1.amount > 0 &&
                    order_idx->state == ORDER_STATE_INIT, "illegal deposit");
            uint64_t liquidity = 0;
            uint64_t token0_used = 0;
            uint64_t token1_used = 0;
            if (pair_idx->reserve0.amount == 0 && pair_idx->reserve1.amount == 0) { //第一笔做市
                liquidity = (uint64_t) floor(sqrt((long double) (order_idx->token0.amount) * (long double) (order_idx->token1.amount)));
                token0_used = order_idx->token0.amount;
                token1_used = order_idx->token1.amount;
            } else {
                token1_used = (uint64_t) ceil((long double) (order_idx->token0.amount) * pair_idx->reserve1.amount / pair_idx->reserve0.amount);
                if (token1_used > order_idx->token1.amount) { //token1不足,使用token0计算
                    token0_used = (uint64_t) ceil((long double) (order_idx->token1.amount) * pair_idx->reserve0.amount / pair_idx->reserve1.amount);
                    token1_used = order_idx->token1.amount;
                } else {
                    token0_used = order_idx->token0.amount;
                }
                liquidity = std::min(
                    floor((long double) (token0_used) * pair_idx->liquidity_token.amount / pair_idx->reserve0.amount), 
                    floor((long double) (token1_used) * pair_idx->liquidity_token.amount / pair_idx->reserve1.amount)
                );
            }
            orders_table.modify(order_idx, owner, [&](auto& row) {
                row.token0_used.amount = token0_used;
                row.token1_used.amount = token1_used;
                row.liquidity_token.amount = liquidity;
                row.state = ORDER_STATE_FINISH;
            });
            pairs_table.modify(pair_idx, _self, [&](auto& row) {
                row.liquidity_token.amount += liquidity;
                row.reserve0.amount += token0_used;
                row.reserve1.amount += token1_used;
            });
            if (token0_used < order_idx->token0.amount) {
                action(
                    permission_level{_self, "active"_n},
                    pair_idx->token0.contract,
                    "transfer"_n,
                    std::make_tuple(get_self(), owner, asset(order_idx->token0.amount - token0_used, order_idx->token0.symbol), std::string("deposit drawback"))
                ).send();
            } else if (token1_used < order_idx->token1.amount) {
                action(
                    permission_level{_self, "active"_n},
                    pair_idx->token1.contract,
                    "transfer"_n,
                    std::make_tuple(get_self(), owner, asset(order_idx->token1.amount - token1_used, order_idx->token1.symbol), std::string("deposit drawback"))
                ).send();
            }
            action(
                permission_level{_self, "active"_n},
                LPTOKEN_CONTRACT,
                "issue"_n,
                std::make_tuple(_self, order_idx->liquidity_token, std::string("deposit"))
            ).send();
            char issue_memo[64] = {0};
            sprintf(issue_memo, "lock,%lld,%s,%s,%lld|%d|%s,%lld|%d|%s", pair_idx->id, owner.to_string().c_str(), inviter.to_string().c_str(),
                order_idx->token0_used.amount, order_idx->token0_used.symbol.precision(), order_idx->token0_used.symbol.code().to_string().c_str(),
                order_idx->token1_used.amount, order_idx->token1_used.symbol.precision(), order_idx->token1_used.symbol.code().to_string().c_str()
            );
            action(
                permission_level{_self, "active"_n},
                LPTOKEN_CONTRACT,
                "transfer"_n,
                std::make_tuple(_self, MINER_CONTRACT, order_idx->liquidity_token, std::string(issue_memo))
            ).send();
            action(
                permission_level{_self, "active"_n},
                _self,
                "liquiditylog"_n,
                std::make_tuple(
                    order_idx->liquidity_token, order_idx->owner, pair_idx->id, order_idx->id, 
                    order_idx->token0_used, order_idx->token1_used, 
                    pair_idx->reserve0, pair_idx->reserve1, pair_idx->liquidity_token
                )
            ).send();
            orders_table.erase(order_idx);
        }

        [[eosio::action]]
        void withdraw(name owner, uint64_t pair_id) {
            require_auth(owner);
            //解锁
            action(
                permission_level{_self, "active"_n},
                MINER_CONTRACT,
                "unlock"_n,
                std::make_tuple(owner, pair_id)
            ).send();
        }

        [[eosio::action]]
        void createlog(uint64_t pair_id, name creator, symbol symbol, token token0, token token1) {
            require_auth(_self);
        }

        [[eosio::action]]
        void liquiditylog(asset liquid, name owner, uint64_t pair_id, uint64_t order_id, asset token0, asset token1, asset reserve0, asset reserve1, asset total_liquidity) {
            require_auth(_self);
        }

        [[eosio::action]]
        void swaplog(name owner, uint64_t pair_id, asset token0, asset fee, asset token1, asset reserve0, asset reserve1, double price) {
            require_auth(_self);
        }

    private:
        using pairs_index = eosio::multi_index<"pairs"_n, pair>;
        using orders_index = eosio::multi_index<"orders"_n, order,
            indexed_by<"owner"_n, const_mem_fun<_order, uint64_t, &order::owner_key>>
        >;
        using config_index = eosio::multi_index<"config"_n, config>;
        using stats = eosio::multi_index< "stat"_n, currency_stats >;
        using swaphistorys_index = eosio::multi_index< "swaphistorys"_n, swaphistory,
            indexed_by<"owner"_n, const_mem_fun<_swaphistory, uint64_t, &swaphistory::owner_key>>
        >;

        pairs_index pairs_table;
        orders_index orders_table;
        config_index config_table;
        swaphistorys_index swaphistorys_table;

        void do_swap(name from, name to, asset quantity, std::string memo) {
            swap_info info = get_swap_info(memo);
            check (quantity.amount > 0, "illegal quantity");
            check (info.pair_id > 0, "illegal swap memo");
            auto pair_idx = pairs_table.find(info.pair_id);
            check (pair_idx != pairs_table.end(), "illegal pair_id");
            auto from_contract = get_first_receiver();
            //手续费
            uint64_t burn_amount = 0;
            uint64_t liquidity_fee = 0;
            uint64_t invite1_fee = 0;
            uint64_t invite2_fee = 0;
            uint64_t library_fee = 0;
            const symbol ech_symbol = symbol(ECH_SYMBOL, ECH_SYMBOL_PRECISION);
            if (from_contract == ECH_CONTRACT && quantity.symbol == ech_symbol) {
                stats statstable( LPTOKEN_CONTRACT, symbol(ECH_SYMBOL, ECH_SYMBOL_PRECISION).code().raw() );
                auto existing = statstable.find( symbol(ECH_SYMBOL, ECH_SYMBOL_PRECISION).code().raw() );
                check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
                const auto& st = *existing;
                if (st.supply.amount > DESTROY_BOTTOM) {
                    burn_amount = (uint64_t)std::min((uint64_t)quantity.amount * SWAP_FEE_BURN / FEE_BASE, (uint64_t)st.supply.amount - DESTROY_BOTTOM);
                }
                liquidity_fee = quantity.amount * SWAP_FEE_LIQUIDITY / FEE_BASE;
                invite1_fee = quantity.amount * SWAP_FEE_INVITE1 / FEE_BASE;
                invite2_fee = quantity.amount * SWAP_FEE_INVITE2 / FEE_BASE;
                library_fee = quantity.amount * SWAP_FEE_LIBRARY / FEE_BASE;
            }
            uint64_t swap_fee = quantity.amount * SWAP_FEE_BACK / FEE_BASE;
            uint64_t mine_fee = liquidity_fee + invite1_fee + invite2_fee;
            uint64_t total_fee = burn_amount + mine_fee + library_fee + swap_fee;
            //换币
            uint64_t swap_amount = quantity.amount - total_fee;
            uint64_t drawback_amount = 0;
            uint64_t reserve0 = pair_idx->reserve0.amount;
            uint64_t reserve1 = pair_idx->reserve1.amount;
            double price0 = pair_idx->price0_last;
            double price1 = pair_idx->price1_last;
            double price = 0;
            symbol drawback_symbol;
            name drawback_contract;
            if (from_contract == pair_idx->token0.contract) {
                check (quantity.symbol == pair_idx->token0.symbol, "illegal asset");
                drawback_amount = pair_idx->reserve1.amount - ceil((long double)(pair_idx->reserve0.amount * pair_idx->reserve1.amount) / (swap_amount + pair_idx->reserve0.amount));
                drawback_symbol = pair_idx->token1.symbol;
                drawback_contract = pair_idx->token1.contract;
                price = price0 = (double)drawback_amount / (double) quantity.amount;
                reserve0 += swap_amount; 
                reserve1 -= drawback_amount;
            } else if (from_contract == pair_idx->token1.contract) {
                check (quantity.symbol == pair_idx->token1.symbol, "illegal asset");
                drawback_amount = pair_idx->reserve0.amount - (pair_idx->reserve0.amount * pair_idx->reserve1.amount) / (swap_amount + pair_idx->reserve1.amount);
                drawback_symbol = pair_idx->token0.symbol;
                drawback_contract = pair_idx->token0.contract;
                price = price1 = (double)drawback_amount / (double) quantity.amount;
                reserve1 += swap_amount; 
                reserve0 -= drawback_amount;
            } else {
                check(false, "illegal asset");
            }
            pairs_table.modify(pair_idx, _self, [&](auto& row) {
                row.price0_last = price0;
                row.price1_last = price1;
                row.reserve0.amount = reserve0;
                row.reserve1.amount = reserve1;
                row.burn_amount.amount += burn_amount;
                row.mine_amount.amount += liquidity_fee + invite1_fee + invite2_fee;
                row.block_time_last = publication_time();
            });
            uint64_t last_id = 0;
            if (swaphistorys_table.begin() != swaphistorys_table.end()) {
                last_id = swaphistorys_table.rbegin()->id;
            }
            uint64_t swaplog_id = last_id + 1;
            swaphistorys_table.emplace(_self, [&](auto& row) {
                row.id = swaplog_id;
                row.owner = from;
                row.sended = quantity;
                row.received = asset(drawback_amount, drawback_symbol);
                row.fee = asset(total_fee, quantity.symbol);
                row.price = price;
                row.create_time = publication_time();
            });
            auto config_idx = config_table.find(CONFIG_ID_TOTAL_SWAPLOGS);
            if (config_idx == config_table.end()) {
                config_idx = config_table.emplace(_self, [&](auto& row) {
                    row.id = CONFIG_ID_TOTAL_SWAPLOGS;
                    row.value = 0;
                });
            }
            if (config_idx->value >= MAX_SWAPLOGS_NUM) {
                if (config_idx->value - MAX_SWAPLOGS_NUM == 0) {
                    swaphistorys_table.erase(swaphistorys_table.begin());
                } else {
                    swaphistorys_table.erase(swaphistorys_table.begin());
                    swaphistorys_table.erase(swaphistorys_table.begin());
                    config_table.modify(config_idx, _self, [&](auto& row) {
                        row.value -= 2;
                    });
                }
            } else {
                config_table.modify(config_idx, _self, [&](auto& row) {
                    row.value ++;
                });
            }
            action(
                permission_level{_self, "active"_n},
                drawback_contract,
                "transfer"_n,
                std::make_tuple(get_self(), from, asset(drawback_amount, drawback_symbol), std::string("swap drawback"))
            ).send();
            if (burn_amount > 0) {
                action(
                    permission_level{_self, "active"_n},
                    ECH_CONTRACT,
                    "transfer"_n,
                    std::make_tuple(_self, ECH_ISSUER, asset(burn_amount, ech_symbol), std::string("swap destroy"))
                ).send();
                action(
                    permission_level{ECH_ISSUER, "active"_n},
                    ECH_CONTRACT,
                    "retire"_n,
                    std::make_tuple(asset(burn_amount, ech_symbol), std::string("swap destroy"))
                ).send();
            }
            if (mine_fee > 0) {
                action(
                    permission_level{_self, "active"_n},
                    ECH_CONTRACT,
                    "transfer"_n,
                    std::make_tuple(_self, MINER_CONTRACT, asset(mine_fee, ech_symbol), std::string("mine,"))
                ).send();
            }
            if (library_fee > 0) {
                action(
                    permission_level{_self, "active"_n},
                    ECH_CONTRACT,
                    "transfer"_n,
                    std::make_tuple(_self, DEFI_LIBRARY, asset(library_fee, ech_symbol), std::string("defi library"))
                ).send();
            }
            action(
                permission_level{_self, "active"_n},
                _self,
                "swaplog"_n,
                std::make_tuple(from, info.pair_id, quantity, asset(total_fee, quantity.symbol), asset(drawback_amount, drawback_symbol), pair_idx->reserve0, pair_idx->reserve1, price)
            ).send();
        }

        void do_deposit(name from, name to, asset quantity, std::string memo) {
            deposit_info info = get_deposit_info(memo);
            check (info.pair_id > 0, "illegal transfer");
            check (info.index == 0 || info.index == 1, "illegal transfer");
            check (quantity.amount > 0, "illegal transfer");
            auto pair_idx = pairs_table.find(info.pair_id);
            check (pair_idx != pairs_table.end(), "illegal pair_id");
            auto from_contract = get_first_receiver();
            orders_index::const_iterator order_idx = orders_table.end();
            if (info.index == 0) {
                uint64_t last_id = 0;
                if (orders_table.begin() != orders_table.end()) {
                    last_id = orders_table.rbegin()->id;
                }
                uint64_t order_id = last_id + 1;
                order_idx = orders_table.emplace(_self, [&](auto& row) {
                    row.id = order_id; 
                    row.owner = from;
                    row.pair_id = pair_idx->id;
                    row.token0 = asset(0l, pair_idx->token0.symbol);
                    row.token0_used = asset(0l, pair_idx->token0.symbol);
                    row.token1 = asset(0l, pair_idx->token1.symbol);
                    row.token1_used = asset(0l, pair_idx->token1.symbol);
                    row.liquidity_token = asset(0l, pair_idx->liquidity_token.symbol);
                    row.state = ORDER_STATE_INIT;
                    row.create_time = publication_time();
                });
            } else {
                order_idx = orders_table.find(orders_table.rbegin()->id);
            }
            check ( order_idx != orders_table.end() && 
                    order_idx->owner == from && 
                    order_idx->pair_id == info.pair_id &&
                    order_idx->state == ORDER_STATE_INIT, "illegal transfer");
            if (from_contract == pair_idx->token0.contract && quantity.symbol == pair_idx->token0.symbol) {
                orders_table.modify(order_idx, _self, [&](auto& row) {
                    row.token0 = quantity;
                });
            } else if (from_contract == pair_idx->token1.contract && quantity.symbol == pair_idx->token1.symbol) {
                orders_table.modify(order_idx, _self, [&](auto& row) {
                    row.token1 = quantity;
                });
            } else {
                check(false, "illegal asset");
            }
        }

        void do_withdraw(name from, name to, asset quantity, std::string memo) {
            withdraw_info info = get_withdraw_info(memo);
            auto pair_idx = pairs_table.find(info.pair_id);
            check (pair_idx != pairs_table.end(), "illegal pair_id");
            auto from_contract = get_first_receiver();
            check (from_contract == LPTOKEN_CONTRACT, "illegal transfer");
            check (quantity.symbol == pair_idx->liquidity_token.symbol, "illegal transfer");
            uint64_t token0 = (uint64_t) floor((long double)quantity.amount * pair_idx->reserve0.amount / pair_idx->liquidity_token.amount);
            uint64_t token1 = (uint64_t) floor((long double)quantity.amount * pair_idx->reserve1.amount / pair_idx->liquidity_token.amount);
            pairs_table.modify(pair_idx, _self, [&](auto& row) {
                row.liquidity_token -= quantity;
                row.reserve0.amount -= token0;
                row.reserve1.amount -= token1;
            });
            if (token0 > 0) {
                action(
                    permission_level{_self, "active"_n},
                    pair_idx->token0.contract,
                    "transfer"_n,
                    std::make_tuple(_self, info.owner, asset(token0, pair_idx->token0.symbol), std::string("withdraw"))
                ).send();
            }
            if (token1 > 0) {
                action(
                    permission_level{_self, "active"_n},
                    pair_idx->token1.contract,
                    "transfer"_n,
                    std::make_tuple(_self, info.owner, asset(token1, pair_idx->token1.symbol), std::string("withdraw"))
                ).send();
            }
            action(
                permission_level{_self, "active"_n},
                LPTOKEN_CONTRACT,
                "retire"_n,
                std::make_tuple(quantity, std::string("withdraw destroy"))
            ).send();
        }
};
