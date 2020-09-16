pipeline {
  	agent any
  	
  	stages {
	    stage('Release Build') {
	      	steps {
				timeout(time: 60, unit: 'MINUTES') 
				{
					sh "git submodule update --init -f"
					sh "mkdir -p release-build ; cd release-build ; cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/dist .."

					dir('release-build') {
						sh "make bootstrap ; make -j ; make -j install"	
					}
					
				}
			}
	    }
		stage('Release Run') {
			
			steps {
				timeout(time: 60, unit: 'MINUTES') 
				{
					dir('release-build') {
						
						withEnv(["LD_LIBRARY_PATH=${pwd()}/dist/lib:${pwd()}/dist/lib64"]) {
							sh "echo $LD_LIBRARY_PATH"
							sh "./dist/testing/run-tests.sh release"
						}
						sh "if grep fail results.log ; then exit -1; else exit 0; fi"
					}					
				}
			}
		}
	}
	post {
		success {
			githubNotify(status: 'SUCCESS', description: 'Jenkins build OK')
		}
		failure {
			githubNotify(status: 'FAILURE', description: 'Jenkins build failed')
		}
	}
}
				   /*mkdir build ; cd build ; cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/dist .. ; make bootstrap; make; make -j install; ln -s ${WORKSPACE}/build/dist/lib/libfabric.so ${WORKSPACE}/build/dist/lib/libfabric.so.1 ; ln -s ${WORKSPACE}/build/dist/lib/libcityhash.so ${WORKSPACE}/build/dist/lib/libcityhash.so.0 ; ln -s ${WORKSPACE}/build/dist/lib/libxpmem.so ${WORKSPACE}/build/dist/lib/libxpmem.so.',
	echo "Build result: ${BUILD_RESULT}"
	*/
	/*RUN_RESULT = sh ( script : 'cd ${WORKSPACE}/build ; export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${WORKSPACE}/build/dist/lib:${WORKSPACE}/build/dist/lib64 ; ./dist/testing/run-tests.sh release &> results.log ; if grep fail results.log ; then echo FAILED; false; else echo SUCCESS; exit 0; fi', returnStatus: true ) == 0
	echo "Run result: ${RUN_RESULT}"*/

