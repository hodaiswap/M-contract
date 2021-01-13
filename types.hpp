#pragma once

#include <eosio/eosio.hpp>
#include "consts.hpp"

using namespace eosio;

/***********************************SWAP****************************************/
struct _token {
    name contract;
    symbol symbol;
};

struct _pair {
    uint64_t id;
    _token token0;
    _token token1;
    asset reserve0;
    asset reserve1;
    asset burn_amount;
    asset mine_amount;
    asset liquidity_token;
    double price0_last;
    double price1_last;
    time_point_sec block_time_last;

    uint64_t primary_key() const { return id;}
};

struct _order {
    uint64_t id;
    name owner;
    uint64_t pair_id;
    asset token0;
    asset token0_used;
    asset token1;
    asset token1_used;
    asset liquidity_token;
    name state;
    time_point_sec create_time;

    uint64_t primary_key() const { return id;}
    uint64_t owner_key() const { return owner.value;}
};

struct _swaphistory {
    uint64_t id;
    name owner;
    asset sended;
    asset received;
    asset fee;
    double price;
    time_point_sec create_time;

    uint64_t primary_key() const { return id;}
    uint64_t owner_key() const { return owner.value;}
};

struct _config {
    uint64_t id;
    uint64_t value;

    uint64_t primary_key() const { return id;}
};

struct deposit_info {
    uint64_t pair_id;
    uint64_t index;
};

struct swap_info {
    uint64_t pair_id;
};

struct withdraw_info {
    uint64_t pair_id;
    name owner;
};

/***********************************TOKEN****************************************/
struct [[eosio::table]] currency_stats {
    asset    supply;
    asset    max_supply;
    name     issuer;

    uint64_t primary_key()const { return supply.symbol.code().raw(); }
};

/***********************************MINER****************************************/
struct _pool {
    uint64_t id;
    name contract;
    asset total_liquidity;
    asset total_invitee1_liquidity;
    asset total_invitee2_liquidity;
    uint128_t acc;
    uint128_t invitee1_acc;
    uint128_t invitee2_acc;

    uint64_t primary_key() const { return id;}
};

struct _lockinfo {
    name owner;
    name inviter1;
    name inviter2;
    name contract;
    asset token0;
    asset token1;
    asset liquidity;
    asset unlock_liquidity;
    asset invitee1_liquidity;
    int invitee1_num;
    uint64_t invitee1_expire_id;
    asset invitee2_liquidity;
    int invitee2_num;
    uint64_t invitee2_expire_id;
    asset debt;
    asset invitee1_debt;
    asset invitee2_debt;
    asset pending;
    asset invitee1_pending;
    asset invitee2_pending;
    time_point_sec lock_time;
    time_point_sec unlock_time;
    time_point_sec expire_at;
    int lock_seconds;

    uint64_t primary_key() const { return owner.value;}
    uint64_t expire_key() const { return expire_at.utc_seconds;}
};

struct _invite_info {
    uint64_t id;
    name owner;
    name inviter;
    asset token0;
    asset token1;
    asset liquidity;
    time_point_sec lock_time;
    time_point_sec unlock_time;
    int lock_seconds;

    uint64_t primary_key() const { return id;}
    uint64_t owner_key() const { return inviter.value;}
};

struct lock_info {
    uint64_t pair_id;
    name owner;
    name inviter;
    asset token0;
    asset token1;
};
