%
% append/3
%

append([],Ys,Ys).
append([X|Xs],Ys,[X|Zs]) :-
    append(Xs,Ys,Zs).

?- append([1,2,3], [4,5,6], Q1).
% Expect: Q1 = [1,2,3,4,5,6]
% Expect: end

%
% member/2
%

member(X, [X|_]).
member(X, [_|Xs]) :- member(X,Xs).
?- member(Q2, [1,2,3]).
% Expect: Q2 = 1
% Expect: Q2 = 2
% Expect: Q2 = 3
% Expect: end

%
% split/3
%

split([],[],[]).
split([A],[A],[]).
split([A,B|Xs],[A|As],[B|Bs]) :-
    split(Xs,As,Bs).

?- split([1,2,3,4,5],Q3,Q4).
% Expect: Q3 = [1,3,5], Q4 = [2,4]
% Expect: end
?- split([1,2,3,4],Q5,Q6).
% Expect: Q5 = [1,3], Q6 = [2,4]
% Expect: end
?- split([],Q7,Q8).
% Expect: Q7 = [], Q8 = []
% Expect: end
?- split(Q9, [1,3], [2,4]).
% Expect: Q9 = [1,2,3,4]
% Expect: end

%
% select/3
%
select([X|Xs], X, Xs).
select([Y|Xs], X, [Y|Ys]) :-
    select(Xs, X, Ys).
?- select([1,2,3,4],2,Q10).
% Expect: Q10 = [1,3,4]
% Expect: end
?- select([1,2,3],Q11,Q12).
% Expect: Q11 = 1, Q12 = [2,3]
% Expect: Q11 = 2, Q12 = [1,3]
% Expect: Q11 = 3, Q12 = [1,2]
% Expect: end

%
% perm/2
%

perm([], []).
perm(Xs, [X|Ys]) :-
    select(Xs, X, Zs),
    perm(Zs, Ys).
?- perm([1,2,3],Q13).
% Expect: Q13 = [1,2,3]
% Expect: Q13 = [1,3,2]
% Expect: Q13 = [2,1,3]
% Expect: Q13 = [2,3,1]
% Expect: Q13 = [3,1,2]
% Expect: Q13 = [3,2,1]
% Expect: end

%
% Check structure indexing
%

actions([], Out, Out).
actions([A|As], In, Out) :-
    action(A, In, Out1),
    actions(As, Out1, Out).

action(shift(Symbol,N), In, Out) :- append(In, [shift(Symbol,N)], Out).
action(goto(Symbol,N), In, Out) :- append(In, [goto(Symbol,N)], Out).
action(reduce(LAs,Cond,Rule), In, Out) :- append(In, [reduce(LAs,Cond,Rule)],Out).

?- actions([shift(a,1),goto(b,2),reduce([x,y,z],cond,rule)], [], Q14).
% Expect: Q14 = [shift(a, 1),goto(b, 2),reduce([x,y,z], cond, rule)]
% Expect: end

%
% Move dot
%

move_dot( ('DOT', X), Symbol ) :-
	!, functor(X, F, _), F \= {}, move_dot_found(X, Symbol).
move_dot( (_, R), Symbol ) :-
	move_dot(R, Symbol).

move_dot_found((Symbol, _), Symbol) :-
	!, functor(Symbol,F,_), F \= {}, \+ is_list(Symbol).
move_dot_found(Symbol, Symbol) :-
	functor(Symbol,F,_), F \= {}, \+ is_list(Symbol).

?- move_dot( ('DOT', subterm_1200, full_stop), Q15).
% Expect: Q15 = subterm_1200
% Expect: end




