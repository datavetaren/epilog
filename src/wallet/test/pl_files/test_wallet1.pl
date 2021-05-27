master_privkey(Master) :-
    current_predicate('$secret' : master/1), '$secret' : master(Master).
master_privkey(Master) :-
    system : password(Password), 
    ec : 
    encrypt(WordList, Password, 2048, 
            encrypt(58'MdDA8UozibA4WHSoZuDz6y35b56qBUC6J8DbSju4ULqhaqx4cWG1ns39A7vPN5DnBChSAFciR8XJaiVzqsPWwTzVL3kHHENxvn9m6ExPjPLaMajn9X2cTtBWpY5LQQb71FW9Kmb91SzmCHFSZmkFT227JYRHZvevh8TBX2KpirjTu3H3RyBQTX9jaJfWSDmSMxfEJAQ2wXvk5Tb77DQQoW2vw6QZHPqbZJVfZcJgRHW9LkiLZj6fCBqg94pjdo88tbFZLKcWDWWfe5tDH7k5diQFKhNLPiF3XzmmDxhvsUFW9Kc9JRhUuKxBKiehiPF7XJWpkEVym4mBBHuNp1PxHLWkzrtvD94gVc6wbecDceYQLZKfybSo7tcs3SiVdCGoydMewRSaHFzxoNPKMkaz3D3PAxhVkLoUq16k7XXNk6pb4GPvoNHZumJzHdj2zmpDeHYvqBwRHKKfskkNQvq8JFnrScPi9rWJe3B9PYRRDc4iBhVPPoM13Xsbhc8Mi5x58mQDiRtpk3ZdpbAT9XuKTsKxRyFTRc2FbQoTPu3AUtxURuWd6j8iPY4tY7wVfHp5dNPA9Vbbq69o8WyjrnuuCMDx9pkxipB53t4K2GC7TBbW2mNdcEBKwCWosb1txBTW9kXW4qcmuGvBJAEpWNfsusJpkgM3NQg4nqK16Ayiza6312LRg9WQ4Q5Vcub1E6xUE9XyM7FiQGw9QpeSsc1r3YUw4qnTccr9Lc2drPz5GmyejMPcDnGcfh8994jtvAX2CmTi8wsTuxdL49CxbajMU6viExxG64TYotb1rjCg9RSejD9k2i57yxP9kPexZxdA2cbxXw5QXhms1uLv75JuovpwJnanjmTrjPJnfbeWU6Tautj2mP2JT26BUaUaoK31
                    )), 
    ec : master_key(WordList, Master, _), assert('$secret' : master(Master)).

master_pubkey(58'xpub661MyMwAqRbcEqPXQ2PncjZFgwdcXtMWbKzsN1gj9vL8dJfBSYmU8T3atuS77LyW1TxoPWZ4Gm1DhCF7j81Xn2pBYcB1QpbCLYca57YjhDT
              ).

pubkey(Count, PubKey) :-
    master_pubkey(Master), 
    ec : child_pubkey(Master, Count, ExtPubKey), 
    ec : normal_key(ExtPubKey, PubKey).

privkey(Count, PrivKey) :-
    master_privkey(Master), 
    ec : child_privkey(Master, Count, ExtPrivKey), 
    ec : normal_key(ExtPrivKey, PrivKey).

lastheap(1796).

numkeys(5).

check_closures(N) :-
    (frozenk(0, 100, HeapAddrs) @ global) @ node, length(HeapAddrs, N).

utxo(1758, tx1, 21000000000, 58'11uia4BfsJzo1QnUnvi2URFmDgcbEbKSYS).
utxo(1796, tx1, 21000000000, 58'11uia4BfsJzo1QnUnvi2URFmDgcbEbKSYS).

