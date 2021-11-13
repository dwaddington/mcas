#!/usr/bin/env python

import math as mt
import numpy as np
from numpy import genfromtxt
import pandas as pd
import time as time
import sys
import sklearn.metrics as mt
from sklearn.linear_model import *
import functools
import math
import pymm

def oracle(S, model) :

    '''
    Train the model and outputs metric for a set of features
    
    INPUTS:
    features -- the feature matrix
    target -- the observations
    S -- index for the features used for model construction
    model -- choose if the regression is linear or logistic
    OUTPUTS:
    float grad -- the garadient of the log-likelihood function
    float log_loss -- the log-loss for the trained model
    float -log_loss -- the negative log-loss, which is proportional to the log-likelihood
    float score -- the R^2 score for the trained linear model
    '''
    # preprocess current solution
    S = np.unique(S[S >= 0])

    # logistic model
    if model == 'logistic' :
        grad, log_like = Logistic_Regression(S)
        return grad, log_like

    # linear model
    if model == 'linear' :
        grad, score = Linear_Regression(S)
        return grad, score


# ------------------------------------------------------------------------------------------
#  logistic regression
# ------------------------------------------------------------------------------------------

def Logistic_Regression(dims):

    '''
    Logistic regression for a given set of features
    
    INPUTS:
    features -- the feature matrix
    target -- the observations
    dims -- index for the features used for model construction
    GRAD -- if set to TRUE the function returns grad
    OUTPUTS:
    float grad -- the garadient of the log-likelihood function
    float log_loss -- the log-loss for the trained model
    '''

    if (s.features[:,dims].size > 0) :
    
        # define sparse features
        sparse_features = np.array(s.features[:,dims])
        if sparse_features.ndim == 1 : sparse_features = sparse_features.reshape(sparse_features.shape[0], 1)
        
        # get model, predict probabilities, and predictions
        model = LogisticRegression(max_iter = 10000).fit(sparse_features , s.target)
        predict_prob  = np.array(model.predict_proba(sparse_features))
        predictions = model.predict(sparse_features)
       
    else :
    
        # predict probabilities, and predictions
        predict_prob  = np.ones((s.features.shape[0], 2)) * 0.5
        predictions = np.ones((s.features.shape[0])) * 0.5

    # conpute gradient of log likelihood
    
    log_like = (-mt.log_loss(s.target, predict_prob) + mt.log_loss(s.target, np.ones((s.features.shape[0], 2)) * 0.5)) * len(target)
    grad = np.dot(s.features.T, s.target - predictions)
    return grad, log_like
      
# ------------------------------------------------------------------------------------------
#  linear regression
# ------------------------------------------------------------------------------------------

def Linear_Regression(dims):

    '''
    Linear regression for a given set of features
    
    INPUTS:
    features -- the feature matrix
    target -- the observations
    dims -- index for the features used for model construction

    OUTPUTS:
    float grad -- the garadient of the log-likelihood function
    float score -- the R^2 score for the trained model
    '''

    # preprocess features and target
    ktime = time.time()
    target = np.array(s.target).reshape(s.target.shape[0], -1)
    if (s.features[:,dims].size > 0) :

        # define sparse features
        sstime = time.time()
        sparse_features = s.features[:,dims]
        if sparse_features.ndim == 1 : sparse_features = sparse_features.reshape(sparse_features.shape[0], 1)
        print ("create sparse_features " + str(time.time() - sstime))

        # get model, predict probabilities, and predictions
        start_time =time.time()
        model = LinearRegression().fit(sparse_features , target)
        print ("linear_regration " + str(time.time() - start_time))

        #predict = model.predict(sparse_features)
        #score = np.sum(target * target) - np.sum((target- predict) * (target- predict))

        start_time =time.time()
        score = model.score(sparse_features , target)
        predict = model.predict(sparse_features)
        print ("score & predict " + str(time.time() - start_time))

    else :
        # predict probabilities, and predictions
        score = 0
        predict = (np.zeros((s.features.shape[0]))).reshape(s.features.shape[0], -1)
    # compute gradient of log likelihood  
    start_time =time.time()
    grad = np.dot(s.features.T, target - predict)
    print ("grad " + str(time.time() - start_time))
    return grad, score



def do_work(model, k) :

    '''
    The SDS algorithm, as in "Submodular Dictionary Selection for Sparse Representation", Krause and Cevher, ICML '10
    
    INPUTS:
    features -- the feature matrix
    target -- the observations
    model -- choose if the regression is linear or logistic
    k -- upper-bound on the solution size
    OUTPUTS:
    float run_time -- the processing time to optimize the function
    int rounds -- the number of parallel calls to the oracle function
    float metric -- a goodness of fit metric for the solution quality
    '''

    # save data to file
    results = pd.DataFrame(data = {'k': np.zeros(k).astype('int'), 'time': np.zeros(k), 'rounds': np.zeros(k),'metric': np.zeros(k)})

    # define time and rounds
    run_time = time.time()
    rounds = 0
    rounds_ind = 0

    # define new solution
    S = np.array([], int)

    for idx in range(k) :
       
        # define and train model
        stime = time.time()
        grad, metric = oracle(S, model)
        oracle_time = time.time()
        rounds += 1
        
        start_point = time.time()
        # define vals
        point = []
        A = np.array(range(len(grad)))
        for a in np.setdiff1d(A, S) :
            point = np.append(point, a)
        out =  [[point, len(np.setdiff1d(A, S))]]
        print (type(out[0]))


        
        print ("pick a point: " +  str(time.time() - start_point))
       
        out = np.array(out, dtype='object')
        rounds_ind += np.max(out[:, -1])
        np_max_time = time.time()
        # save results to file
        results.loc[idx,'k']      = idx + 1
        results.loc[idx,'time']   = time.time() - run_time
        results.loc[idx,'rounds'] = int(rounds)
        results.loc[idx,'rounds_ind'] = rounds_ind
        results.loc[idx,'metric'] = metric

    
    
       # get feasible points
        points = np.array([])
        points = np.append(points, np.array(out[0, 0]))
        points = points.astype('int')
        e = time.time()
        # break if points are no longer feasible
        if len(points) == 0 : break
        
        # otherwise add maximum point to current solution
        print (grad)
        exit()
        a = points[0]
        for i in points :
            if grad[i] > grad[a] :
                a = i
                
        if grad[a] >= 0 :
            S  = np.unique(np.append(S,i))
        else : break
        f = time.time()
        print ("oracle_time " + str(oracle_time - stime))
        print ("np_max_time " + str(np_max_time - oracle_time))
        print ("round time " + str(f - stime))
        print ("----- ")
        
    # update current time
    run_time = time.time() - run_time
    print (results)
    return results

'''
Test algorithms with the run_experiment function.
target -- the observations for the regression
features -- the feature matrix for the regression
model -- choose if 'logistic' or 'linear' regression
k_range -- range for the parameter k for a set of experiments
'''

'''
Linear Regration
'''

# define features and target for the experiments


shelf_size_GB = 20
shelf_name = str(shelf_size_GB) + "GBshelf" # 20GBshelf
s = pymm.shelf(shelf_name,size_mb=shelf_size_GB*1024,pmem_path='/mnt/pmem0')

print (s.features.shape)

# initalize features and target

# choose if logistic or linear regression
model = 'linear'

# set range for the experiments
k_range = np.array([50])

# run experiment
do_work(model, k_range[-1])

                                                 

