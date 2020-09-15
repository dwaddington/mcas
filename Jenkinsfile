pipeline {
  	agent any
  	stages {
	    stage('Build') {
	      steps {
				timeout(time: 60, unit: 'MINUTES') 
				{
					sh '''cd ${WORKSPACE} ; git submodule update --init -f ; ls ${WORKSPACE}/src/lib/GSL
					cd ${WORKSPACE} ; mkdir build ; cd build ; cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/dist ..
					cd ${WORKSPACE}/build ; make bootstrap ; make
					'''
				}
			}
	    }
		stage('Run') {
			steps {
				timeout(time: 60, unit: 'MINUTES') 
				{
					sh '''cd ${WORKSPACE}/build
					export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${WORKSPACE}/build/dist/lib:${WORKSPACE}/build/dist/lib64
					./dist/testing/run-tests.sh release &> results.log
					if grep fail results.log ; then echo FAILED; false; else echo SUCCESS; exit 0; fi
					'''
					node {
						def rfile1 = new File("${WORKSPACE}/build/results.log")
						def lines = rfile1.readLines()
						def error_found = lines.find{ line-> line =~ /FAILED/ }
						if (error_found) {
							currentBuild.result = "FAILURE"
							throw new Exception("Run failure stopped pipeline")
						}
					}

				}
			}
		}
				   /*mkdir build ; cd build ; cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/dist .. ; make bootstrap; make; make -j install; ln -s ${WORKSPACE}/build/dist/lib/libfabric.so ${WORKSPACE}/build/dist/lib/libfabric.so.1 ; ln -s ${WORKSPACE}/build/dist/lib/libcityhash.so ${WORKSPACE}/build/dist/lib/libcityhash.so.0 ; ln -s ${WORKSPACE}/build/dist/lib/libxpmem.so ${WORKSPACE}/build/dist/lib/libxpmem.so.',
	echo "Build result: ${BUILD_RESULT}"
	*/
	/*RUN_RESULT = sh ( script : 'cd ${WORKSPACE}/build ; export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${WORKSPACE}/build/dist/lib:${WORKSPACE}/build/dist/lib64 ; ./dist/testing/run-tests.sh release &> results.log ; if grep fail results.log ; then echo FAILED; false; else echo SUCCESS; exit 0; fi', returnStatus: true ) == 0
	echo "Run result: ${RUN_RESULT}"*/


	    stage('Github Notify') {
	      steps {
	        githubNotify(status: 'SUCCESS', description: 'Jenkins build OK')
	      }
	    }

  }
}
