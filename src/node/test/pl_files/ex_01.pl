?- member(X, [1,10]).
% Expect: X = 1
% Expect: X = 10
% Expect: end

% Meta: WAM-only

pkey(58'KxyDY2J8WmDYi72DwkVLhMYofAvfRDTociyHRpiTvAu1mY51sCoh).
pkey2(58'L15fZcjA6ABYayBAQRPvTEZMYQ4WZ19y2fCzdZ3FNjny9aCNB7SZ).

%
% Initial mycoin in Address.
%

?- pkey(S), ec:pubkey(S, P), ec:address(P, Address),
   me:commit(tx('$mycoin'(10,_), Hash, tx1, args(Sign,PubKey,Address), _)).
% Expect: true/*

%
% Transfer that mycoin to a new address.
%

?- pkey(S),
   ec:pubkey(S,PubKey),
   pkey2(S2),
   ec:pubkey(S2,PubKey2), ec:address(PubKey2,NewAddress),
   Script = (Sign = MySignature,
	     (p(Hash) :- frozenk(-1,1,[H]),
	                 defrost(H, Closure, [Hash, args(Sign,PubKey,Address)]),
			 arg(4, Closure, MyCoin),
			 tx(MyCoin, Hash0, tx1, args(_,_,NewAddress), _))),
   arg(2, Script, ScriptBody),
   ec:hash(ScriptBody, ScriptHash),
   ec:sign(S, ScriptHash, MySignature),
   ec:validate(PubKey, ScriptHash, MySignature),
   me:commit(Script).
% Expect: true/*
