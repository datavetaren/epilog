![Epilog](/images/epilog.png)

Mission: Combining Prolog, blockchain and cryptocurrency.

"Prolog was the beginning. Epilog is the end."

## Latest Epilog binaries (automatically updated)

* [Linux Ubuntu 22](https://github.com/datavetaren/epilog/releases/download/master/epilog_linux_ubuntu_22_latest.zip)
* [Mac OSX Monterey](https://github.com/datavetaren/epilog/releases/download/master/epilog_macosx_monterey_latest.zip)
* [Windows 10/11](https://github.com/datavetaren/epilog/releases/download/master/epilog_windows_latest.zip)

## How to build from source

Go to some directory:

```
> git clone https://github.com/datavetaren/epilog
> git clone https://github.com/datavetaren/secp256k1-zkp
> [install C++ Boost, 1.74 or later]
> cd epilog
> make all
```

## How to run

In two separate terminal windows do:
```
> bin/epilogd --name foo --port 8701 --interactive
> bin/epilogd --name bar --port 8702 --interactive
```

(You can launch multiple engines on the same machine on different
ports.)

There are some special commands and operators:

List current connections:

``` 
?- connections.
```

Add another epilogd to your address book:

```
?- add_address('127.0.0.1', 8702).
```
(It'll get connected within a minute.)

```
?- member(X, [1,2,3,4]) @ bar.
```

Run the goal "member(X, [1,2,3,4])" on the node 'bar'. (Requires a
connection to the node 'bar'.)

Use the regular 'halt/0' predicate to quit (interactive) epilogd:

```
?- halt.
```

For fun you can actually create compatible bitcoin keys with epilogd:

```
?- privkey(NewKey), pubkey(NewKey,PubKey), address(PubKey, Addr), bc1(PubKey,BC1).            
NewKey = 58'KxT19dNhBFbtm4kQgzSoTf6CBnoTyqsWAm2avZ9HrT6nxfQx8HU6,
PubKey = 58'1qPpPqtGiSy9AZ4rVuTRC5N2Pz2ynGbjejq2Zr2iYriei,
Addr = 58'1CvaN5tzcpowy2swPGrsjyhh6eyH5PnaA6,
BC1 = "bc1qst950vj87th6fs62gjfepxpytj7p9u6tkq08jj".
```

## What is Prolog?

Prolog is a language based on predicates (= "program") and terms (=
"data structures.") Predicates become logical axioms for which you can
ask queries: "Solve X,Y,Z such as p(X,Y,Z) is true."  It turns out
that this system is Turing machine complete, and yes although it means
you can create Pacman in Prolog, it is more useful for solving other
problems.

## Why Prolog?

Prolog is a Turing machine complete database based on predicate logic,
a branch in mathematics. It's a very high level language; complicated
things can be succintly expressed and solved. Some key features are:

* Code in Prolog has the same representation as data.
* Cryptocurrency is about maintaining a consistent state.
* Logic helps in smart contracts.
* Immutable datastructures. Fits nicely with the "blockchain paradigm."
* Binding variables = spending.
* Frozen closures are mapped to UTXO spending and smart contracts execution.
* Pragmatic. Although based on its mathematical foundation it's also pragmatic.
* ...

## What is Epilog?

(First study Prolog, then...)

Imagine that you have a Prolog session:

```
?- <type something here>
```

Imagine that the whole world shares this prompt. You type something
and it gets concatenated to a never ending query with everybody elses
typing. Each goal gets incrementally solved. Miners bundle the queries
together in chunks (blocks.) No one really hits "enter," or "enter" is
continually being pressed between blocks, but the state persists. You
are only allowed to append to the neverending query as long it does
not fail which should remind you about the definition of logical
consistency in mathematics. This is basically it. This is the
"operating system" that underpins everything.

In this operating system there's also a scarce resource.  As Prolog is
fully programmable you can define arbitrary rules on how this scarce
resource should be exchanged between parties. You can also maintain
states via logic variables which enhance the expressiveness of smart
contracts. Prolog is also very succint which means that smart
contracts become compact and keeping things compact means increased
scalability.

## Money supply and premine

The total supply is 4,242,424,242,424,242 EPS, that's 42 eight
times. The genesis block reward goes to the founders (aka premine)
which is 42,445,243,724,242 EPS (~ 1%.) The remaining reward is
21000000000 >> (height / 100000) with the same block interval as
bitcoin (10 minutes.)  Inflation tapers twice as fast as bitcoin, or a
halving event every other year. Last reward will be in block
3,500,000.

## Implementation

Epilog is written in C++. It contains a complete Prolog implementation
with extensions. We've struggled hard to achieve a minimum number of
external dependencies: only boost and libsecp256k1 (from bitcoin.)
This is intentional as it becomes less fragile for full node
validation; consensus programming is highly non-trivial. Like bitcoin,
our goal is to never use hard forks. Hard forks means centralization
as it gives the impression that "developers are in control." We think
nobody should be in control.

If you're a Prolog expert and knows C++ and want to get involved in
this exciting project, then drop an email at below. I'm in the process
of assembling an international team. Let's compete with everything but
bitcoin.

## Contact

Datavetaren

Email: ```datavetaren@datavetaren.se```
