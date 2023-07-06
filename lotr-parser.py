import os
import os
import numpy as np
import pickle
import re
import time
import sys
datadir = './04-crypto-sc/data/'

if __name__ == '__main__':
    if len(sys.argv) < 2:
        datadir +='rsa'
        prefix = 'rsa'
    else:
        datadir += sys.argv[1]
        prefix = sys.argv[1]
    onefiles = os.listdir(datadir + '-1-parsed')
    onelist = []
    for fname in onefiles:
        with open(datadir + '-1-parsed/' + fname, 'r') as f:
            onelist.append([int(n) for n in f])
    print(len(onelist))
    onethreshold = np.percentile(np.array([len(l) for l in onelist]),0.3)

    zerofiles = os.listdir(datadir + '-0-parsed')
    zerolist = []
    for fname in zerofiles:
        with open(datadir + '-0-parsed/' + fname, 'r') as f:
            zerolist.append([int(n) for n in f])
    print(len(zerolist))
    zerothreshold = np.percentile(np.array([len(l) for l in zerolist]),0.3)
    print((onethreshold, zerothreshold))
    threshold = int(min(zerothreshold, onethreshold))
    newzero = [z for z in zerolist if len(z) >= threshold]
    newone = [o for o in onelist if len(o) >= threshold]
    dsize = min((len(newzero),len(newone)))
    x_arr = np.zeros([dsize*2, threshold],dtype=np.float32)
    y_arr = np.arange(dsize*2)%2
    for i in range(dsize):
        x_arr[2*i] = newzero[i][:threshold]
        x_arr[2*i+1] = newone[i][:threshold]
    print(x_arr.shape)
    std = 16
    mean = np.median(x_arr)
    x_arr2 = (x_arr-mean)/std
    print((mean,))
    train_x = x_arr2[:2000]
    train_y = y_arr[:2000]
    valid_x = x_arr2[2000:4000]
    valid_y = y_arr[2000:4000]
    test_x = x_arr2[2000:5000]
    test_y = y_arr[2000:5000]
    from sklearn import svm
    clf = svm.SVC(kernel = 'rbf',gamma=0.1, C=100)

    clf.fit(train_x, train_y)
    pred_y = clf.predict(test_x)
    #tpred_y = clf.predict(train_x)
    print((pred_y == test_y).sum()/len(pred_y))
    if os.path.isfile(prefix+".pkl"):
        x_arr_old, y_arr_old = pickle.load(open(prefix+".pkl",'rb'))
        newlen = min(x_arr_old.shape[1], x_arr.shape[1])
        print("Old tracelen {}, new tracelen {}".format(x_arr_old.shape[1], newlen))
        x_arr_new = np.concatenate([x_arr_old[:,:newlen], x_arr[:,:newlen]], axis=0)
        y_arr_new = np.concatenate([y_arr_old, y_arr])
    else:
        x_arr_new = x_arr
        y_arr_new = y_arr
    print("total num traces: " + str(len(y_arr_new)))
    traincut = 7*(len(y_arr_new)//10)
    valcut = 17*(len(y_arr_new)//20)
    pickle.dump((x_arr_new, y_arr_new), open(prefix+".pkl", "wb"))
    pickle.dump((x_arr_new[:traincut], y_arr_new[:traincut]), open(prefix+"_train.pkl", "wb"))
    pickle.dump((x_arr_new[traincut:valcut], y_arr_new[traincut:valcut]), open(prefix+"_valid.pkl", "wb"))
    pickle.dump((x_arr_new[valcut:], y_arr_new[valcut:]), open(prefix+"_test.pkl", "wb"))


