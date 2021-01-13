#include <string>
#include <algorithm>

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

#include "../methods.hpp"

//using namespace std;
using namespace eosio;

class [[eosio::contract]] miner : public contract {
    public:
        using contract::contract;

        struct [[eosio::table]] pool : public _pool {
        };
        struct [[eosio::table]] lockinfo : public _lockinfo {
        };
        struct [[eosio::table]] invite1 : public _invite_info {
        };
        struct [[eosio::table]] invite2 : public _invite_info {
        };
        struct [[eosio::table]] order : public _invite_info {
        };
        struct [[eosio::table]] config : public _config {
        };

        miner(name receiver, name code, datastream<const char*> ds) : 
            contract(receiver, code, ds)
        {
        }

        [[eosio::on_notify("*::transfer")]] 
        void transfer(name from, name to, asset quantity, std::string memo) {
            if (from == _self) return;
            if(to != _self) return;
            check (from == SWAP_CONTRACT, "illegal lock");
            if (is_lock(memo)) {
                do_lock(from, to, quantity, memo);
            } else if (is_mine(memo)) {
                do_mine(from, to, quantity, memo); 
            } else {
                check (false, "illegal transfer");
            }
        }
        
        [[eosio::action]]
        void unlock(name owner, uint64_t pair_id) {
            pools_index pools_table(_self, _self.value);
            auto pool_idx = pools_table.find(pair_id);
            check (pool_idx != pools_table.end(), "illegal pool_info");
            lockinfo_index lockinfo_table(_self, pair_id);
            auto lockinfo_idx = lockinfo_table.find(owner.value);
            check (lockinfo_idx != lockinfo_table.end(), "illegal unlock");
            check (lockinfo_idx->lock_time.utc_seconds + lockinfo_idx->lock_seconds <= time_point_sec(publication_time()).utc_seconds, "illegal unlock");
            char memo[64] = {0};
            sprintf(memo, "withdraw,%lld,%s", pair_id, owner.to_string().c_str());
            action(
                permission_level{_self, "active"_n},
                lockinfo_idx->contract,
                "transfer"_n,
                std::make_tuple(_self, SWAP_CONTRACT, lockinfo_idx->liquidity, std::string(memo))
            ).send();
            asset expire_liquidity = asset(0, lockinfo_idx->liquidity.symbol);
            asset expire_invitee1 = asset(0, lockinfo_idx->liquidity.symbol);
            asset expire_invitee2 = asset(0, lockinfo_idx->liquidity.symbol);
            if (lockinfo_idx->expire_at.utc_seconds != -1) { //还未被处理过
                if (lockinfo_idx->inviter1 != lockinfo_idx->owner) {
                    auto invitee1_idx = lockinfo_table.find(lockinfo_idx->inviter1.value);
                    if (invitee1_idx != lockinfo_table.end()) {
                        lockinfo_table.modify(invitee1_idx, _self, [&](auto& row) {
                            row.invitee2_pending.amount += pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO - row.invitee1_debt.amount;
                            row.invitee1_debt.amount = pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO;
                            row.invitee1_liquidity -= lockinfo_idx->liquidity;
                            expire_invitee1 += lockinfo_idx->liquidity;
                        });
                    }
                }
                if (lockinfo_idx->inviter2 != lockinfo_idx->owner) {
                    auto invitee2_idx = lockinfo_table.find(lockinfo_idx->inviter2.value);
                    if (invitee2_idx != lockinfo_table.end()) {
                        lockinfo_table.modify(invitee2_idx, _self, [&](auto& row) {
                            row.invitee2_pending.amount += pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO - row.invitee2_debt.amount;
                            row.invitee2_debt.amount = pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO;
                            row.invitee2_liquidity -= lockinfo_idx->liquidity;
                            expire_invitee2 += lockinfo_idx->liquidity;
                        });
                    }
                }
                lockinfo_table.modify(lockinfo_idx, _self, [&](auto& row) {
                    row.pending.amount += pool_idx->acc * row.liquidity.amount / MINE_RADIO - row.debt.amount;
                    row.debt.amount = pool_idx->acc * row.liquidity.amount / MINE_RADIO;
                    expire_liquidity += row.liquidity;
                    row.invitee1_pending.amount += pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO - row.invitee1_debt.amount;
                    row.invitee1_debt.amount = pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO;
                    expire_invitee1 += lockinfo_idx->invitee1_liquidity;
                    row.invitee2_pending.amount += pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO - row.invitee2_debt.amount;
                    row.invitee2_debt.amount = pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO;
                    expire_invitee2 += lockinfo_idx->invitee2_liquidity;
                    row.unlock_liquidity.amount = 0;
                });
            }
            asset withdraw_amount = lockinfo_idx->pending + lockinfo_idx->invitee1_pending + lockinfo_idx->invitee2_pending;
            if (withdraw_amount.amount > 0) {
                action(
                    permission_level{_self, "active"_n},
                    ECH_CONTRACT,
                    "transfer"_n,
                    std::make_tuple(_self, owner, withdraw_amount, std::string("user withdraw"))
                ).send();
            }
            pools_table.modify(pool_idx, _self, [&](auto& row) {
                row.total_liquidity -= expire_liquidity;
                row.total_invitee1_liquidity -= expire_invitee1;
                row.total_invitee2_liquidity -= expire_invitee2;
            });
            if (lockinfo_idx->expire_at.utc_seconds != -1 && lockinfo_idx->lock_time.utc_seconds + lockinfo_idx->lock_seconds <= time_point_sec(publication_time()).utc_seconds) {
                lockinfo_table.modify(lockinfo_idx, _self, [&](auto& row) {
                    row.token0.amount = 0;
                    row.token1.amount = 0;
                    row.liquidity.amount = 0;
                    row.unlock_liquidity.amount = 0;
                    row.debt.amount = 0;
                    row.pending.amount = 0;
                    row.lock_seconds = 0;
                    row.unlock_time = publication_time();
                });
            } else {
                lockinfo_table.erase(lockinfo_idx);
            }
        }

        [[eosio::action]]
        void withdraw(name owner, uint64_t pair_id) {
            pools_index pools_table(_self, _self.value);
            auto pool_idx = pools_table.find(pair_id);
            check(pool_idx != pools_table.end(), "illegal pair_id");
            lockinfo_index lockinfo_table(_self, pair_id);
            auto lockinfo_idx = lockinfo_table.find(owner.value);
            check (lockinfo_idx != lockinfo_table.end(), "illegal withdraw");
            asset pending = asset(0, lockinfo_idx->pending.symbol);
            asset invitee1_pending = asset(0, lockinfo_idx->invitee1_pending.symbol);
            asset invitee2_pending = asset(0, lockinfo_idx->invitee2_pending.symbol);
            lockinfo_table.modify(lockinfo_idx, _self, [&](auto& row) {
                if (row.expire_at.utc_seconds != -1) {
                    row.pending.amount += pool_idx->acc * row.liquidity.amount / MINE_RADIO - row.debt.amount;
                    row.debt.amount = pool_idx->acc * row.liquidity.amount / MINE_RADIO;
                    pending = row.pending;
                    row.invitee1_pending.amount += pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO - row.invitee1_debt.amount;
                    row.invitee1_debt.amount = pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO;
                    invitee1_pending = row.invitee1_pending;
                    row.invitee2_pending.amount += pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO - row.invitee2_debt.amount;
                    row.invitee2_debt.amount = pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO;
                    invitee2_pending = row.invitee2_pending;
                    row.pending.amount = 0;
                    row.invitee1_pending.amount = 0;
                    row.invitee2_pending.amount = 0;
                }
            });
            action(
                permission_level{_self, "active"_n},
                ECH_CONTRACT,
                "transfer"_n,
                std::make_tuple(_self, owner, pending + invitee1_pending + invitee2_pending, std::string("withdraw"))
            ).send();
        }

    private:
        using lockinfo_index = eosio::multi_index<"lockinfo"_n, lockinfo,
            indexed_by<"expire"_n, const_mem_fun<_lockinfo, uint64_t, &lockinfo::expire_key>>
        >;
        using pools_index = eosio::multi_index<"pools"_n, pool>;
        using orders_index = eosio::multi_index<"orders"_n, order,
            indexed_by<"owner"_n, const_mem_fun<_invite_info, uint64_t, &order::owner_key>>     
        >;
        using invite1_index = eosio::multi_index<"invite1"_n, invite1,
            indexed_by<"owner"_n, const_mem_fun<_invite_info, uint64_t, &invite1::owner_key>>     
        >;
        using invite2_index = eosio::multi_index<"invite2"_n, invite2,
            indexed_by<"owner"_n, const_mem_fun<_invite_info, uint64_t, &invite2::owner_key>>     
        >;
        using config_index = eosio::multi_index<"config"_n, config>;

        void do_lock(name from, name to, asset quantity, std::string memo) {
            lock_info info = get_lock_info(memo);
            pools_index pools_table(_self, _self.value);
            auto pool_idx = pools_table.find(info.pair_id);
            asset debt = asset(0, symbol("ECH", 4));
            asset invitee1_debt = asset(0, symbol("ECH", 4));
            asset invitee2_debt = asset(0, symbol("ECH", 4));
            if (pool_idx != pools_table.end()) {
                debt.amount = pool_idx->acc * pool_idx->total_liquidity.amount / MINE_RADIO;
                invitee1_debt.amount = pool_idx->invitee1_acc * pool_idx->total_invitee1_liquidity.amount / MINE_RADIO;
                invitee2_debt.amount = pool_idx->invitee2_acc * pool_idx->total_invitee2_liquidity.amount / MINE_RADIO;
            }
            lockinfo_index lockinfo_table(_self, info.pair_id);
            auto inviter1_lockinfo_idx = lockinfo_table.end();
            if (pool_idx != pools_table.end() && pool_idx->total_liquidity.amount != 0) { //如果已经存在了则一定要提供一个邀请者
                inviter1_lockinfo_idx = lockinfo_table.find(info.inviter.value);
                check (inviter1_lockinfo_idx != lockinfo_table.end(), "illegal inviter");
                check (inviter1_lockinfo_idx->expire_at.utc_seconds != -1, "inviter already expired");
                check (info.inviter != info.owner, "illegal inviter");
            }
            auto inviter2_lockinfo_idx = lockinfo_table.end();
            if (inviter1_lockinfo_idx != lockinfo_table.end() && inviter1_lockinfo_idx->owner != inviter1_lockinfo_idx->inviter1 && inviter1_lockinfo_idx->expire_at.utc_seconds != -1) {
                inviter2_lockinfo_idx = lockinfo_table.find(inviter1_lockinfo_idx->inviter1.value);
            }
            auto lockinfo_idx = lockinfo_table.find(info.owner.value);
            if (lockinfo_idx == lockinfo_table.end()) {
                invite1_index invite1_table(_self, _self.value);
                invite2_index invite2_table(_self, _self.value);
                lockinfo_idx = lockinfo_table.emplace(_self, [&](auto& row) {
                    row.owner = info.owner;
                    if (inviter1_lockinfo_idx != lockinfo_table.end()) {
                        row.inviter1 = inviter1_lockinfo_idx->owner;
                    } else {
                        row.inviter1 = row.owner;
                    }
                    if (inviter2_lockinfo_idx != lockinfo_table.end()) {
                        row.inviter2 = inviter2_lockinfo_idx->owner;
                    } else {
                        row.inviter2 = row.owner;
                    }
                    row.contract = get_first_receiver();
                    row.token0 = info.token0;
                    row.token1 = info.token1;
                    row.liquidity = quantity;
                    row.unlock_liquidity = asset(0, info.token0.symbol);
                    row.invitee1_liquidity = asset(0, quantity.symbol);
                    row.invitee1_num = 0;
                    if (invite1_table.begin() != invite1_table.end()) {
                        row.invitee1_expire_id = invite1_table.rbegin()->id + 1;
                    } else {
                        row.invitee1_expire_id = 0;
                    }
                    row.invitee2_liquidity = asset(0, quantity.symbol);
                    row.invitee2_num = 0;
                    if (invite2_table.begin() != invite2_table.end()) {
                        row.invitee2_expire_id = invite2_table.rbegin()->id + 1;
                    } else {
                        row.invitee2_expire_id = 0;
                    }
                    row.debt = debt;
                    row.invitee1_debt = invitee1_debt;
                    row.invitee2_debt = invitee2_debt;
                    row.pending = asset(0, symbol("ECH", 4));
                    row.invitee1_pending = asset(0, symbol("ECH", 4));
                    row.invitee2_pending = asset(0, symbol("ECH", 4));
                    row.lock_time = publication_time();
                    row.lock_seconds = ORDER_LOCK_SECONDS;
                    row.expire_at = time_point_sec(row.lock_time.utc_seconds + ORDER_LOCK_SECONDS + EXPIRE_KEEP_SECONDS);
                });
            } else {
                lockinfo_table.modify(lockinfo_idx, _self, [&](auto& row) {
                    check (row.lock_time.utc_seconds + row.lock_seconds <= time_point_sec(publication_time()).utc_seconds, "unlock first");
                    check (row.unlock_time.utc_seconds > 0, "unlock first");
                    if (inviter1_lockinfo_idx != lockinfo_table.end()) {
                        row.inviter1 = inviter1_lockinfo_idx->owner;
                    } else {
                        row.inviter1 = row.owner;
                    }
                    if (inviter2_lockinfo_idx != lockinfo_table.end()) {
                        row.inviter2 = inviter2_lockinfo_idx->owner;
                    } else {
                        row.inviter2 = row.owner;
                    }
                    row.token0 += info.token0;
                    row.token1 += info.token1;
                    row.liquidity += quantity;
                    row.pending += debt - row.debt;
                    row.debt = debt;
                    row.lock_time = publication_time();
                    if (row.unlock_liquidity >= row.token0 * ORDER_UNLOCK_RADIO) {
                        row.lock_seconds = 0;
                        row.expire_at.utc_seconds = time_point_sec(publication_time()).utc_seconds + EXPIRE_KEEP_SECONDS;
                    } else {
                        row.lock_seconds = ORDER_LOCK_SECONDS;
                        row.expire_at = time_point_sec(row.lock_time.utc_seconds + ORDER_LOCK_SECONDS + EXPIRE_KEEP_SECONDS);
                    }
                    row.unlock_time.utc_seconds = 0;
                });
            }
            {
                orders_index orders_table(_self, _self.value);
                uint64_t last_id = 0;
                if (orders_table.begin() != orders_table.end()) {
                    last_id = orders_table.rbegin()->id;
                }
                uint64_t current_id = last_id + 1;
                orders_table.emplace(_self, [&](auto& row) {
                    row.id = current_id;
                    row.owner = info.owner;
                    row.inviter = info.owner;
                    row.token0 = info.token0;
                    row.token1 = info.token1;
                    row.liquidity = quantity;
                    row.lock_time = publication_time();
                    row.lock_seconds = ORDER_LOCK_SECONDS;
                });
                config_index config_table(_self, _self.value);
                auto config_idx = config_table.find(CONFIG_ID_ORDER_NUM);
                if (config_idx == config_table.end()) {
                    config_idx = config_table.emplace(_self, [&](auto& row) {
                        row.id = CONFIG_ID_ORDER_NUM;
                        row.value = 0;
                    });
                }
                if (config_idx->value >= MAX_ORDER_NUM) {
                    if (config_idx->value - MAX_ORDER_NUM == 0) {
                        orders_table.erase(orders_table.begin());
                    } else {
                        orders_table.erase(orders_table.begin());
                        orders_table.erase(orders_table.begin());
                        config_table.modify(config_idx, _self, [&](auto& row) {
                            row.value -= 2;
                        });
                    }
                } else {
                    config_table.modify(config_idx, _self, [&](auto& row) {
                        row.value ++;
                    });
                }
            }
            if (inviter1_lockinfo_idx != lockinfo_table.end()) {
                lockinfo_table.modify(inviter1_lockinfo_idx, _self, [&](auto& row) {
                    row.invitee1_liquidity += quantity;
                    row.invitee1_num += 1;
                    row.invitee1_pending += invitee1_debt - row.invitee1_debt;
                    row.invitee1_debt = invitee1_debt;
                    if (inviter1_lockinfo_idx->lock_time.utc_seconds + inviter1_lockinfo_idx->lock_seconds > time_point_sec(publication_time()).utc_seconds) {
                        row.unlock_liquidity += info.token0;
                    }
                    if (row.unlock_liquidity >= row.token0 * ORDER_UNLOCK_RADIO) {
                        row.lock_seconds = 0;
                        row.expire_at.utc_seconds = time_point_sec(publication_time()).utc_seconds + EXPIRE_KEEP_SECONDS;
                    }
                });
                invite1_index invite1_table(_self, _self.value);
                uint64_t last_id = 0;
                if (invite1_table.begin() != invite1_table.end()) {
                    last_id = invite1_table.rbegin()->id;
                }
                uint64_t current_id = last_id + 1;
                invite1_table.emplace(_self, [&](auto& row) {
                    row.id = current_id;
                    row.owner = info.owner;
                    row.inviter = inviter1_lockinfo_idx->owner;
                    row.token0 = info.token0;
                    row.token1 = info.token1;
                    row.liquidity = quantity;
                    row.lock_time = publication_time();
                    row.lock_seconds = ORDER_LOCK_SECONDS;
                });
                config_index config_table(_self, _self.value);
                auto config_idx = config_table.find(CONFIG_ID_INVITE1_NUM);
                if (config_idx == config_table.end()) {
                    config_idx = config_table.emplace(_self, [&](auto& row) {
                        row.id = CONFIG_ID_INVITE1_NUM;
                        row.value = 0;
                    });
                }
                if (config_idx->value >= MAX_INVITE1_NUM) {
                    if (config_idx->value - MAX_INVITE1_NUM == 0) {
                        invite1_table.erase(invite1_table.begin());
                    } else {
                        invite1_table.erase(invite1_table.begin());
                        invite1_table.erase(invite1_table.begin());
                        config_table.modify(config_idx, _self, [&](auto& row) {
                            row.value -= 2;
                        });
                    }
                } else {
                    config_table.modify(config_idx, _self, [&](auto& row) {
                        row.value ++;
                    });
                }
            }
            if (inviter2_lockinfo_idx != lockinfo_table.end()) {
                lockinfo_table.modify(inviter2_lockinfo_idx, _self, [&](auto& row) {
                    row.invitee2_liquidity += quantity;
                    row.invitee2_num += 1;
                    row.invitee2_pending += invitee2_debt - row.invitee2_debt;
                    row.invitee2_debt = invitee1_debt;
                });
                invite2_index invite2_table(_self, _self.value);
                uint64_t last_id = 0;
                if (invite2_table.begin() != invite2_table.end()) {
                    last_id = invite2_table.rbegin()->id;
                }
                uint64_t current_id = last_id + 1;
                invite2_table.emplace(_self, [&](auto& row) {
                    row.id = current_id;
                    row.owner = info.owner;
                    row.inviter = inviter2_lockinfo_idx->owner;
                    row.token0 = info.token0;
                    row.token1 = info.token1;
                    row.liquidity = quantity;
                    row.lock_time = publication_time();
                    row.lock_seconds = ORDER_LOCK_SECONDS;
                });
                config_index config_table(_self, _self.value);
                auto config_idx = config_table.find(CONFIG_ID_INVITE2_NUM);
                if (config_idx == config_table.end()) {
                    config_idx = config_table.emplace(_self, [&](auto& row) {
                        row.id = CONFIG_ID_INVITE2_NUM;
                        row.value = 0;
                    });
                }
                if (config_idx->value >= MAX_INVITE2_NUM) {
                    if (config_idx->value - MAX_INVITE2_NUM == 0) {
                        invite2_table.erase(invite2_table.begin());
                    } else {
                        invite2_table.erase(invite2_table.begin());
                        invite2_table.erase(invite2_table.begin());
                        config_table.modify(config_idx, _self, [&](auto& row) {
                            row.value -= 2;
                        });
                    }
                } else {
                    config_table.modify(config_idx, _self, [&](auto& row) {
                        row.value ++;
                    });
                }
            }
            if (pool_idx == pools_table.end()) {
                pools_table.emplace(_self, [&](auto& row) {
                    row.id = info.pair_id;
                    row.contract = get_first_receiver();
                    row.total_liquidity = quantity;
                    if (inviter1_lockinfo_idx != lockinfo_table.end()) {
                        row.total_invitee1_liquidity = quantity;
                    } else {
                        row.total_invitee1_liquidity = asset(0, quantity.symbol);
                    }
                    if (inviter2_lockinfo_idx != lockinfo_table.end()) {
                        row.total_invitee2_liquidity = quantity;
                    } else {
                        row.total_invitee2_liquidity = asset(0, quantity.symbol);
                    }
                    row.acc = 0;
                    row.invitee1_acc = 0;
                    row.invitee2_acc = 0;
                });
            } else {
                pools_table.modify(pool_idx, _self, [&](auto& row) {
                    row.total_liquidity += quantity;
                    if (inviter1_lockinfo_idx != lockinfo_table.end()) {
                        row.total_invitee1_liquidity += quantity;
                    }
                    if (inviter2_lockinfo_idx != lockinfo_table.end()) {
                        row.total_invitee2_liquidity += quantity;
                    }
                });
            }
        }

        void do_mine(name from, name to, asset quantity, std::string memo) {
            pools_index pools_table(_self, _self.value);
            auto pool_idx = pools_table.begin(); //只会有一个池子参与挖矿
            check (pool_idx != pools_table.end(), "illegal mine");
            asset expire_liquidity = asset(0, pool_idx->total_liquidity.symbol);
            asset expire_invitee1 = asset(0, pool_idx->total_invitee1_liquidity.symbol);
            asset expire_invitee2 = asset(0, pool_idx->total_invitee2_liquidity.symbol);
            lockinfo_index lockinfo_table(_self, pool_idx->id);
            auto idx = lockinfo_table.get_index<"expire"_n>();
            int current_num = 0;
            time_point_sec current_time = publication_time();
            invite1_index invite1_table(_self, _self.value);
            invite2_index invite2_table(_self, _self.value);
            while (idx.begin() != idx.end() && current_num ++ < MAX_EXPIRE_DEAL_NUM) {
                auto lockinfo_idx = idx.begin();
                if (lockinfo_idx->expire_at.utc_seconds > current_time.utc_seconds) {
                    break;
                }
                if (lockinfo_idx->unlock_time.utc_seconds != 0) { //已经unlock了
                    idx.erase(lockinfo_idx); 
                    continue;
                }
                if (lockinfo_idx->inviter1 != lockinfo_idx->owner) {
                    auto invitee1_idx = lockinfo_table.find(lockinfo_idx->inviter1.value);
                    if (invitee1_idx != lockinfo_table.end()) {
                        lockinfo_table.modify(invitee1_idx, _self, [&](auto& row) {
                            row.invitee2_pending.amount += pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO - row.invitee1_debt.amount;
                            row.invitee1_debt.amount = pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO;
                            row.invitee1_liquidity -= lockinfo_idx->liquidity;
                            expire_invitee1 += lockinfo_idx->liquidity;
                        });
                    }
                }
                if (lockinfo_idx->inviter2 != lockinfo_idx->owner) {
                    auto invitee2_idx = lockinfo_table.find(lockinfo_idx->inviter2.value);
                    if (invitee2_idx != lockinfo_table.end()) {
                        lockinfo_table.modify(invitee2_idx, _self, [&](auto& row) {
                            row.invitee2_pending.amount += pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO - row.invitee2_debt.amount;
                            row.invitee2_debt.amount = pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO;
                            row.invitee2_liquidity -= lockinfo_idx->liquidity;
                            expire_invitee2 += lockinfo_idx->liquidity;
                        });
                    }
                }
                idx.modify(lockinfo_idx, _self, [&](auto& row) {
                    row.expire_at.utc_seconds = -1; //设置成最大值
                    row.pending.amount += pool_idx->acc * row.liquidity.amount / MINE_RADIO - row.debt.amount;
                    row.debt.amount = pool_idx->acc * row.liquidity.amount / MINE_RADIO;
                    expire_liquidity += lockinfo_idx->liquidity;
                    //row.liquidity.amount = 0;
                    row.invitee1_pending.amount += pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO - row.invitee1_debt.amount;
                    row.invitee1_debt.amount = pool_idx->invitee1_acc * row.invitee1_liquidity.amount / MINE_RADIO;
                    expire_invitee1 += lockinfo_idx->invitee1_liquidity;
                    //row.invitee1_liquidity.amount = 0;
                    //row.invitee1_num = 0;
                    row.invitee2_pending.amount += pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO - row.invitee2_debt.amount;
                    row.invitee2_debt.amount = pool_idx->invitee2_acc * row.invitee2_liquidity.amount / MINE_RADIO;
                    expire_invitee2 += lockinfo_idx->invitee2_liquidity;
                    //row.invitee2_liquidity.amount = 0;
                    //row.invitee2_num = 0;
                    row.unlock_liquidity.amount = 0;
                    //row.token0.amount = 0;
                    //row.token1.amount = 0;
                });
            }
            asset inviter1_amount = quantity * SWAP_FEE_INVITE1 / (SWAP_FEE_LIQUIDITY + SWAP_FEE_INVITE1 + SWAP_FEE_INVITE2);
            asset inviter2_amount = quantity * SWAP_FEE_INVITE2 / (SWAP_FEE_LIQUIDITY + SWAP_FEE_INVITE1 + SWAP_FEE_INVITE2);
            asset liquidity_amount = quantity - inviter1_amount - inviter2_amount;
            pools_table.modify(pool_idx, _self, [&](auto& row) {
                row.total_liquidity -= expire_liquidity;
                row.total_invitee1_liquidity -= expire_invitee1;
                row.total_invitee2_liquidity -= expire_invitee2;
                if (row.total_liquidity.amount != 0) {
                    row.acc += liquidity_amount.amount * MINE_RADIO / row.total_liquidity.amount;
                }
                if (row.total_invitee1_liquidity.amount != 0) {
                    row.invitee1_acc += inviter1_amount.amount * MINE_RADIO / row.total_invitee1_liquidity.amount;
                }
                if (row.total_invitee2_liquidity.amount != 0) {
                    row.invitee2_acc += inviter2_amount.amount * MINE_RADIO / row.total_invitee2_liquidity.amount;
                }
            });
        }

};
