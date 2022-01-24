"""
    Problem:    Predict house selling price
    Data:       house_data.txt
    Algorithm:  Multivariate linear regression
"""

import sys
import math
import numpy as np

def scalefeatures(data, m, n):
    mean = [0] + [(sum([data[i][j] for i in range(m)])) / float(m)
                  for j in range(1, n + 1)]

    stddeviation = [0] + [math.sqrt(sum(
                                        [(data[i][j] - mean[j]) ** 2
                                         for i in range(m)]
                                        ) / float(m))
                         for j in range(1, n + 1)]

    for j in range(1, n + 1):
        for i in range(m):
            data[i][j] = (data[i][j] - mean[j]) / stddeviation[j]

    return data


def h(theta, x, n):
    """
        theta: hypothesis [theta0, theta1, ..., theta_n)
        x: training example [1, x1, x2, ..., x_n]
    """
    return sum([theta[i] * x[i] for i in range(n + 1)])


def cost(theta, x, y, m, n):
    summation = sum([(h(theta, x[i], n) - y[i]) ** 2
                        for i in range(m)])
    return summation / (2 * m)

def predict(theta, x, m, n):
    print (x[0])
    pred = [0] * 1 # TBD should be m
    for j in range (1): # TBD  should be m
        pred[j] = [theta[i] * x[j][i] for i in range(1, n + 1)] 
    print (pred)
    return pred

def gradientdescent(theta, x, y, m, n, alpha, iterations):
    for i in range(iterations):
        thetatemp = theta[:]
        for j in range(n + 1):
            print ("we have here an error")
            print (m)
            print (len(x))
            print (len(y))
            summation = sum([(h(theta, x[k], n) - y[k]) * x[k][j]
                             for k in range(m)])
            thetatemp[j] = thetatemp[j] - alpha * summation / m
        theta = thetatemp[:]
#        print (theta)
    return theta


def main():
    x = []  # List of training example parameters
    y = []  # List of training example results

#    for line in sys.stdin:
#        data = map(float, line.split(','))
#        x.append(data[:-1])
#        y.append(data[-1])
    x.append ([2.3, 2.2 ,2.5, 44]) 
    x.append ([3.2, 2.4 ,2.5, 44]) 
    x.append ([3.3, 2.7 ,4.5, 2])
    y.append (1.0)
    y.append (0.4)
    y.append (0.3)
    do_work(x, y)

def do_work(x , y):
    print (x)
    m = len(x)      # Number of training examples
    n = len(x[0])   # Number of features
    print (n)
    # Append a column of 1's to x
    x = [[1] + i for i in x]
    # Initialize learning parameters
    initialtheta = [0.0] * (n + 1)
    learningrate = 0.01
    iterations = 100

#    x = scalefeatures(x, m, n)
    # Run gradient descent to get our guessed hypothesis
    finaltheta = gradientdescent(initialtheta,
                                 x, y, m, n,
                                 learningrate, iterations)
    print ("len(finaltheta)")
    # Evaluate our hypothesis accuracy
    final_cost = cost(finaltheta, x, y, m, n)
    print (x)
    final_predict = np.array(predict(finaltheta, x, m, n)).T
    print ("Initial cost: ", cost(initialtheta, x, y, m, n))
    print ("Final cost: ", cost(finaltheta, x, y, m, n))
    print ("predict: ", predict(finaltheta, x,  m, n))
    return (final_cost, final_predict)

if __name__ == "__main__":
    main()
