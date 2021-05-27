%
% ec:privkey/1
%
% This key was generated from bitaddress.org
%
?- ec:privkey(58'L326Y3N3XHcGWSnhiTPZTb544aGZt6x8sTfLpnWKwoeLr3NWghct).
% Expect: true

%
% Make sure it matches the bitcoin address given by bitaddress.org
%

?- ec:pubkey(58'L326Y3N3XHcGWSnhiTPZTb544aGZt6x8sTfLpnWKwoeLr3NWghct, X), ec:address(X, Y).
% Expect: X = 58'1semYvast3hpYyTLioxNqwwL9WNXJqfrRqUJ5xurzeMrV, Y = 58'1LxVHoMAuAJbznVT7gjdyT1of42fGCrZNZ.

%
% Sign something (foobar(frotz(42)) using this private key and get the
% signature.
%

?- ec:sign(58'L326Y3N3XHcGWSnhiTPZTb544aGZt6x8sTfLpnWKwoeLr3NWghct, foobar(frotz(42)), Sign).
% Expect: Sign = 58'3DwsrzHDTpx4XRjF8tV3bkhrH2J5oBzSQpai5TKauHdAptDb5zVDdXTZvyNNUr7TVCayAa8eefk6LEqMsQwJizmp

%
% Verify that the signature is correct using the public key on the same
% message.
%

?- ec:validate(58'1semYvast3hpYyTLioxNqwwL9WNXJqfrRqUJ5xurzeMrV, foobar(frotz(42)), 58'3DwsrzHDTpx4XRjF8tV3bkhrH2J5oBzSQpai5TKauHdAptDb5zVDdXTZvyNNUr7TVCayAa8eefk6LEqMsQwJizmp).
% Expect: true
