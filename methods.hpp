#pragma once

#include <memory>

#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>

#include "types.hpp"

/***********************************SWAP****************************************/
bool is_deposit(const std::string& memo) {
    return memo.length() >= DEPOSIT_LENGTH && memcmp(memo.data(), DEPOSIT, DEPOSIT_LENGTH) == 0;
}

deposit_info get_deposit_info(const std::string& memo) {
    deposit_info res = {0l, 0l};
    int pos1 = 0;
    int pos2 = 0;
    for (int i = 0; i < memo.length(); i ++) {
        if (memo[i] == ',') {
            if (pos1 == 0) pos1 = i;
            else if (pos2 == 0) pos2 = i;
            if (pos1 != 0 && pos2 != 0) break;
        }
    }
    if (pos2 == 0) pos2 = memo.length();
    char tmp[64] = {0};
    memcpy(tmp, memo.data() + pos1 + 1, pos2 - pos1 - 1);
    res.pair_id = atoll(tmp);
    if (pos2 != memo.length()) {
        memset(static_cast<void*>(tmp), 0, 64);
        memcpy(tmp, memo.data() + pos2 + 1, memo.length() - pos2 - 1);
        res.index = atoll(tmp);
    }
    return res;
}

bool is_swap(const std::string& memo) {
    return memo.length() >= SWAP_LENGTH && memcmp(memo.data(), SWAP, SWAP_LENGTH) == 0;
}

swap_info get_swap_info(const std::string& memo) {
    swap_info res = {0l};
	if (memo.length() <= SWAP_LENGTH || memcmp(memo.data(), SWAP, SWAP_LENGTH) != 0) {
        return res;
    }
    res.pair_id = atoll(memo.data() + SWAP_LENGTH);
	return res;
}

bool is_withdraw(const std::string& memo) {
    return memo.length() >= WITHDRAW_LENGTH && memcmp(memo.data(), WITHDRAW, WITHDRAW_LENGTH) == 0;
}

withdraw_info get_withdraw_info(const std::string& memo) {
    withdraw_info res = {0l, ""_n};
    int pos1 = 0;
    int pos2 = 0;
    for (int i = 0; i < memo.length(); i ++) {
        if (memo[i] == ',') {
            if (pos1 == 0) pos1 = i;
            else if (pos2 == 0) pos2 = i;
            if (pos1 != 0 && pos2 != 0) break;
        }
    }
    if (pos2 == 0) pos2 = memo.length();
    char tmp[64] = {0};
    memcpy(tmp, memo.data() + pos1 + 1, pos2 - pos1 - 1);
    res.pair_id = atoll(tmp);
    if (pos2 != memo.length()) {
        memset(static_cast<void*>(tmp), 0, 64);
        memcpy(tmp, memo.data() + pos2 + 1, memo.length() - pos2 - 1);
        res.owner = name(tmp);
    }
    return res;
}

symbol create_lptoken_symbol(uint64_t id, uint16_t precision) {
	char str[LPTOKEN_SYMBOL_MAX + 1] = {0}; //目前最多有三位数
	unsigned unum = id;
	int i=0,j; 
	do { 
		str[i++] = LPTOKEN_SYMBOL[unum % (unsigned)LPTOKEN_SYMBOL_RADIX]; 
		unum /= LPTOKEN_SYMBOL_RADIX; 
	} while(unum); 
	str[i] = '\0'; 
	char temp; 
	for(j=0; j<=(i-1)/2; j++) { 
		temp = str[j]; 
		str[j] = str[i-j-1]; 
		str[i-j-1] = temp; 
	} 
    char symbol_code[LPTOKEN_SYMBOL_BASE_LENGTH] = {0};
    strcpy(symbol_code, LPTOKEN_SYMBOL_BASE);
    for (int k = strlen(symbol_code), j = i - 1; j >= 0; j--) {
        symbol_code[--k] = str[j];
    }
	return symbol(symbol_code, precision);
}

/***********************************MINER****************************************/
bool is_mine(const std::string& memo) {
    return memo.length() >= MINE_LENGTH && memcmp(memo.data(), MINE, MINE_LENGTH) == 0;
}

bool is_lock(const std::string& memo) {
    return memo.length() >= LOCK_LENGTH && memcmp(memo.data(), LOCK, LOCK_LENGTH) == 0;
}

lock_info get_lock_info(const std::string& memo) {
    lock_info res = {0l, ""_n, ""_n};
    int pos1 = 0;
    int pos2 = 0;
    int pos3 = 0;
    int pos4 = 0;
    int pos5 = 0;
    for (int i = 0; i < memo.length(); i ++) {
        if (memo[i] == ',') {
            if (pos1 == 0) pos1 = i;
            else if (pos2 == 0) pos2 = i;
            else if (pos3 == 0) pos3 = i;
            else if (pos4 == 0) pos4 = i;
            else if (pos5 == 0) pos5 = i;
            if (pos1 != 0 && pos2 != 0 && pos3 != 0 && pos4 != 0 && pos5 != 0) break;
        }
    }
    check (pos1 != 0 && pos2 != 0 && pos3 != 0 && pos4 != 0 && pos5 != 0, "illegal lock");
    char tmp[64] = {0};
    memcpy(tmp, memo.data() + pos1 + 1, pos2 - pos1 - 1);
    res.pair_id = atoll(tmp);
    memset(static_cast<void*>(tmp), 0, 64);
    memcpy(tmp, memo.data() + pos2 + 1, pos3 - pos2 - 1);
    res.owner = name(tmp);
    memset(static_cast<void*>(tmp), 0, 64);
    memcpy(tmp, memo.data() + pos3 + 1, pos4 - pos3 - 1);
    res.inviter = name(tmp);
    memset(static_cast<void*>(tmp), 0, 64);
    memcpy(tmp, memo.data() + pos4 + 1, pos5 - pos4 - 1);
    {
        int p1 = 0;
        int p2 = 0;
        for (int i = 0; i < 64; i ++) {
            if (tmp[i] == '|') {
                if (p1 == 0) p1 = i;
                else if (p2 == 0) p2 = i;
                if (p1 != 0 && p2 != 0) break;
            }
        }
        check (p1 != 0 && p2 != 0, "illegal lock");
        char t[64] = {};
        memcpy(t, tmp, p1);
        uint64_t amount = atoll(t);
        memset(static_cast<void*>(t), 0, 64);
        memcpy(t, tmp + p1 + 1, p2 - p1 -1);
        int precision = atoi(t);
        std::string s(tmp + p2 + 1);
        res.token0 = asset(amount, symbol(s, precision));
    }
    memset(static_cast<void*>(tmp), 0, 64);
    memcpy(tmp, memo.data() + pos5 + 1, memo.length() - pos5 - 1);
    {
        int p1 = 0;
        int p2 = 0;
        for (int i = 0; i < 64; i ++) {
            if (tmp[i] == '|') {
                if (p1 == 0) p1 = i;
                else if (p2 == 0) p2 = i;
                if (p1 != 0 && p2 != 0) break;
            }
        }
        check (p1 != 0 && p2 != 0, "illegal lock");
        char t[64] = {};
        memcpy(t, tmp, p1);
        uint64_t amount = atoll(t);
        memset(static_cast<void*>(t), 0, 64);
        memcpy(t, tmp + p1 + 1, p2 - p1 -1);
        int precision = atoi(t);
        std::string s(tmp + p2 + 1);
        res.token1 = asset(amount, symbol(s, precision));
    }
    return res;
}

