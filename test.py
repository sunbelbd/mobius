import numpy as np
from interface import *

data=np.random.rand(10000,100)
query=np.random.rand(1000,100)

factor = 2.0;

# parameters: (num, dim, index_vectors, return_top_k, search_budget, factor)
# search_budget should be no less than return_top_k
mobius_ipwrap = MobiusMIPS(data.shape[0],data.shape[1],data,100,200,factor)

# some parameters can also be modified after constructed an instance of MobiusMIPS
# For example: return_k and search_budget
# We can enumerate different search_budget to obtain the desired recall-speed tradeoff
mobius_ipwrap.return_k = 100
mobius_ipwrap.search_budget = 100


for q in range(query.shape[0]):
    score, id = mobius_ipwrap.search(query[q])
    print(score, id)

