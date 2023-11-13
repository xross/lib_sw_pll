@Library('xmos_jenkins_shared_library@v0.23.0') _


getApproval()


pipeline {
    agent none

    options {
        disableConcurrentBuilds()
        skipDefaultCheckout()
        timestamps()
        // on develop discard builds after a certain number else keep forever
        buildDiscarder(logRotator(
            numToKeepStr:         env.BRANCH_NAME ==~ /develop/ ? '25' : '',
            artifactNumToKeepStr: env.BRANCH_NAME ==~ /develop/ ? '25' : ''
        ))
    }
    parameters {
        string(
            name: 'TOOLS_VERSION',
            defaultValue: '15.2.1',
            description: 'The XTC tools version'
        )
    }
    environment {
        PYTHON_VERSION = "3.10.5"
        VENV_DIRNAME = ".venv"
    }

    stages {
        stage('Build and tests') {
            agent {
                label 'linux&&64'
            }
            stages{
                stage('Checkout'){
                    steps {
                        sh 'mkdir lib_sw_pll'
                        // source checks require the directory
                        // name to be the same as the repo name
                        dir('lib_sw_pll') {
                            // checkout repo
                            checkout scm
                            installPipfile(false)
                            withVenv {
                                withTools(params.TOOLS_VERSION) {
                                    sh './tools/ci/checkout-submodules.sh'
                                }
                            }
                        }
                    }
                }
                stage('Build'){
                    steps {
                        dir('lib_sw_pll') {
                            withVenv {
                                withTools(params.TOOLS_VERSION) {
                                    sh './tools/ci/do-ci-build.sh'
                                }
                            }
                        }
                    }
                }
                stage('Test'){
                    steps {
                         dir('lib_sw_pll') {
                            withVenv {
                                withTools(params.TOOLS_VERSION) {
                                    catchError {
                                        sh './tools/ci/do-ci-tests.sh'
                                    }
                                    zip archive: true, zipFile: "build.zip", dir: "build"
                                    zip archive: true, zipFile: "tests.zip", dir: "tests/bin"
                                    archiveArtifacts artifacts: "tests/bin/timing-report.txt", allowEmptyArchive: false

                                    junit 'tests/results.xml'
                                }
                            }
                        }
                    }
                }
            }
            post {
                cleanup {
                    xcoreCleanSandbox()
                }
            }
        }
    }
}
