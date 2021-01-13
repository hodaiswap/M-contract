all:
	cd swap && eosio-cpp swap.cpp -o swap.wasm -I.
	cd miner && eosio-cpp miner.cpp -o miner.wasm -I.
dev:
	cd swap && eosio-cpp swap.cpp -o swap.wasm -I. -DDEV
	cd miner && eosio-cpp miner.cpp -o miner.wasm -I. -DDEV
clean:
	cd miner && rm -f *.abi && rm -f *.wasm
	cd swap && rm -f *.abi && rm -f *.wasm
