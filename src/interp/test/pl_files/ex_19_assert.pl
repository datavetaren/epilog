% Meta: WAM-only
% Meta: stdlib
% Meta: fileio on

%
% Testing assert/retract
%

:- assert(foo(1)).
:- assert(foo(2)).
:- assert(foo(3)).

update_loop :-
    retract(foo(X)), X1 is X + 1, assert(foo(X1)), fail.
update_loop :- true.
   
?- update_loop, findall(X, foo(X), L).
% Expect: L = [2,3,4]

%
% Testing assert with partial match on clauses
%
:- assert(bar(1)).
:- assert(bar(1) :- true).

update_bar(X) :-
    retract((bar(X) :- true)),
    write('update_bar: '), write(X), nl.

?- findall(X, update_bar(X), L).
% Expect: L = [1]

%
% Testing assert with partial match on clause with body
%
:- assert(bar2(1)).
:- assert(bar2(1) :- true).

update_bar2(X) :-
    retract(bar2(X)),
    write('update_bar2: '), write(X), nl.

?- findall(X, update_bar2(X), L).
% Expect: L = [1]

:- assert(baz(10) :- true).
:- assert(baz(10)).
:- assert((baz(20) :- write(20),nl)).

?- retractall(baz(10)), baz(X).
% Expect: X = 20
% Expect: end

