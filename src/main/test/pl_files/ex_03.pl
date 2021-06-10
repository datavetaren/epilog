% Meta: WAM-only

%
% Erase all existing nodes & wallets
%

?- erase_all.
% Expect: true

%
% Create 3 nodes where one will be a miner node.
%

?- start(node, n1, 9000), start(node, n2, 9001), start(node, miner, 9002), nolimit @ node(n1), nolimit @ node(n2), nolimit @ node(miner).
% Expect: true

%
% Set proof of work to simple
%
?- pow_mode(simple) @ node(n1), pow_mode(simple) @ node(n2), pow_mode(simple) @ node(miner).
% Expect: true

%
% Let nodes connect
%
?- add_address('127.0.0.1', 9000) @ node(n2), add_address('127.0.0.1', 9001) @ node(n1), add_address('127.0.0.1', 9000) @ node(miner), add_address('127.0.0.1', 9002) @ node(n1).
% Expect: true
    

%
% Create wallets and connect to nodes
%

?- start(wallet, w1), start(wallet, w2), start(wallet, wm).
% Expect: true

%
% Connect wallets to nodes
%
?- connect(wallet(w1), node(n1)), connect(wallet(w2), node(n2)), connect(wallet(wm), node(miner)).
% Expect: true


%
% Wallets
%
% X = [skate,glory,blur,park,famous,cattle,bread,drift,arch,track,absurd,hospital,estate,sock,laundry,regular,rely,neutral,quiz,coil,exchange,slogan,human,choose]
% X = [define,menu,year,glad,accident,tail,scissors,ostrich,only,place,coast,always,guilt,lizard,once,evoke,iron,nothing,explain,burger,elbow,spy,dog,comic]
% X = [zero,brain,mule,tide,aerobic,wall,lift,mechanic,custom,crash,drill,leopard, vivid,library,pizza,trouble,reward,goat,quote,like,brave,lens,animal,area]
%
    
?- create("w1", [skate,glory,blur,park,famous,cattle,bread,drift,arch,track,absurd,hospital,estate,sock,laundry,regular,rely,neutral,quiz,coil,exchange,slogan,human,choose]) @ wallet(w1).
% Expect: true

?- create("w2", [define,menu,year,glad,accident,tail,scissors,ostrich,only,place,coast,always,guilt,lizard,once,evoke,iron,nothing,explain,burger,elbow,spy,dog,comic]) @ wallet(w2).
% Expect: true

?- create("wm", [zero,brain,mule,tide,aerobic,wall,lift,mechanic,custom,crash,drill,leopard, vivid,library,pizza,trouble,reward,goat,quote,like,brave,lens,animal,area]) @ wallet(wm).
% Expect: true
    
%
% Wait until all nodes are fully connected
%
wait_connections(Node) :-
    sleep(1000), connections(C) @ node(Node), length(C, N), (N < 2 -> wait_connections(Node) ; true).
    
?- wait_connections(n1), wait_connections(n2), wait_connections(miner).
% Expect: true

%
% Create a the first block reward, send it to miner's wallet.
%
?- newkey(A,B) @ wallet(wm), assert(tmp:miner(B)).
% Expect: true/*

?- setup_commit @ node(miner), tmp:miner(MinerAddr), commit(reward(MinerAddr)) @ node(miner).
% Expect: true/*

% PoW this block
?- pow @ node(miner).
% Expect: true/*

% Sync
?- sync @ wallet(wm), balance(B) @ wallet(wm).
% Expect: true/*

% Get receiving addresses for wallet w1 & w2.
?- newkey(A1, B1) @ wallet(w1), assert(tmp:wallet1(0,B1)), newkey(A2, B2) @ wallet(w2), assert(tmp:wallet2(0,B2)).
% Expect: true/*

% Miner sends coins to these wallets
?- tmp:wallet1(0, A), tmp:wallet2(0, B), (password("wm"), spend_many([A,B],[1_000_000, 2_000_000], 300_000, Tx, RemoveUtxos)) @ wallet(wm), retract_utxos(RemoveUtxos) @ wallet(wm), commit(Tx) @- node(miner).
% Expect: true/*

% Check new balance
?- (sync, balance(B)) @ wallet(wm).
% Expect: true/*

% Get block
?- (tip(Tip), block(Tip, Block, Info)) @ node(miner).
% Expect: true/*





