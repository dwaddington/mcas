pipeline {
  agent any
  stages {
    stage('Build') {
      steps {
        sh '''cd ${WORKSPACE}
git submodule update --init --recursive

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/dist ..
make bootstrap
make
make -j install
ln -s ${WORKSPACE}/build/dist/lib/libfabric.so ${WORKSPACE}/build/dist/lib/libfabric.so.1
ln -s ${WORKSPACE}/build/dist/lib/libcityhash.so ${WORKSPACE}/build/dist/lib/libcityhash.so.0
ln -s ${WORKSPACE}/build/dist/lib/libxpmem.so ${WORKSPACE}/build/dist/lib/libxpmem.so.0

cd ${WORKSPACE}/build
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${WORKSPACE}/build/dist/lib:${WORKSPACE}/build/dist/lib64
./dist/testing/run-tests.sh release &> results.log

if grep fail results.log
then
	echo FAILED
	exit 101
else
	echo SUCCESS
	exit 0
fi'''
      }
    }

    stage('Github Notify') {
      steps {
        githubNotify(status: 'SUCCESS', description: 'Jenkins build OK')
      }
    }

  }
}