% Meta: WAM-only

% Meta: erase all test wallets

% Remove global DB
?- drop_global @ node.
% Expect: true

?- (nolimit, heartbeat) @ node.
% Expect: true

%
% Wallets
%
% X = [skate,glory,blur,park,famous,cattle,bread,drift,arch,track,absurd,hospital,estate,sock,laundry,regular,rely,neutral,quiz,coil,exchange,slogan,human,choose]
%
% X = [define,menu,year,glad,accident,tail,scissors,ostrich,only,place,coast,always,guilt,lizard,once,evoke,iron,nothing,explain,burger,elbow,spy,dog,comic]
%
%

?- file("test_wallet1.pl").
% Expect: true/*    
    
?- create("miner", [skate,glory,blur,park,famous,cattle,bread,drift,arch,track,absurd,hospital,estate,sock,laundry,regular,rely,neutral,quiz,coil,exchange,slogan,human,choose]).
% Expect: true/*

?- newkey(A,B), assert(tmp:monieee(B)).
% Expect: true/*

%
% Create rewards with 2 UTXOs
% Then we can spend two times
%    
?- tmp:monieee(Addr), commit(reward(Addr)) @ node, commit(reward(Addr)) @ node.
% Expect: true/*

?- sync.
% Expect: true/*

?- balance(X).
% Expect: X = 42466243724242.

?- save.
% Expect: true/*

?- chain @ node.
% Expect: true/*

%
% Wallet 1 now has a lot of money
%

%
% Create new wallet 2
%

?- file("test_wallet2.pl").
% Expect: true/*

?- create("foobar2", [define,menu,year,glad,accident,tail,scissors,ostrich,only,place,coast,always,guilt,lizard,once,evoke,iron,nothing,explain,burger,elbow,spy,dog,comic]).
% Expect: true/*

?- newkey(A,Addr), assert(tmp:wallet2addr(Addr)) @ wallet2.
% Expect: true/*

%
% Switch back to wallet 1
%

?- file("test_wallet1.pl").
% Expect: true/*

% Create burner address    
?- ec:privkey(Priv), ec:pubkey(Priv, Pub), ec:address(Pub, Addr), assert(tmp:burnaddr(Addr)).
% Expect: true/*

%
% Create transaction with fee
%
?- password("miner"), tmp:burnaddr(Addr), spend_one(Addr, 100000, 1234, Tx, OldUtxos), retract_utxos(OldUtxos), spend_one(Addr, 200000, 9999, Tx2, OldUtxos2), retract_utxos(OldUtxos2), tmp:wallet2addr(Wallet2) @ wallet2, TxR = (Tx, (Tx2, reward(Wallet2))), wrapfee(TxR, Fee, tx(Fee, _, tx1, args(_,_,Wallet2), _), FinalTx) @ node, write(FinalTx), nl, commit(FinalTx) @- node.
% Expect: true/*

%
% Let's see if reward + fee is in wallet2
%
    
?- file("test_wallet2.pl").
% Expect: true/*
?- sync.
% Expect: true/*
?- balance(B), write(B), nl, R = 21000000000, F is 1234 + 9999, Q is R + F, write(Q), nl, B = Q.
% Expect: true/*
