# Locking Rules

MCAS supports read and write locks on kv-pairs.


|*client*       |  *shard pre-op*|      *ADO*    |  *shard post-op*|
|---------------|----------------|---------------|-----------------|
|put            |  wr-lock       |      n/a      |  wr-unlock      |
|get            |  rd-lock       |      n/a      |  rd-unlock      |
|put-direct     |  wr-lock       |      n/a      |  wr-unlock      |
|get-direct     |  rd-lock       |      n/a      |  rd-unlock      |
|invoke-ado     |  wr-lock       |      {1}      |  wr-unlock      |
|invoke-put-ado |  wr-lock       |      {1}      |  wr-unlock      |


## Constraints:
{1} ADO may only lock/unlock kv-pairs other than target pair.


# ADO Operations


|*callback*   |  *target pre-condition*  | *target post-condition*   |
|-------------|--------------------------|---------------------------|
|create-key   |  unlocked                | wr-lock                   |
|open-key     |                          | wr-lock                   |
|resize-value |  unlocked                | unlocked                  |
|resize-value |  wr-lock                 | wr-lock                   |
|unlock-key   |  wr-lock                 | unlocked                  |
|unlock-key   |  rd-lock                 | rd-lock/unlocked          |

