// This file is for internal AMD use.
// If you are interested in running your own Jenkins, please raise a github issue for assistance.

def runCompileCommand(platform, project, jobName, boolean debug=false, boolean staticLibrary=false) {
    project.paths.construct_build_prefix()

    String buildTypeArg = debug ? '-DCMAKE_BUILD_TYPE=Debug' : '-DCMAKE_BUILD_TYPE=Release'
    String buildTypeDir = debug ? 'debug' : 'release'
    String installDKMS = 'sudo apt install amdgpu-dkms'

    if (platform.jenkinsLabel.contains('rhel')) {
        installDKMS = 'sudo yum install amdgpu-dkms'
    }
    else if (platform.jenkinsLabel.contains('sles')) {
        installDKMS = 'sudo zypper install amdgpu-dkms'
    }

    def command = """#!/usr/bin/env bash
                set -x
                echo Build rocDecode - ${buildTypeDir}
                cd ${project.paths.project_build_prefix}
                ${installDKMS}
                python rocDecode-setup.py
                mkdir -p build/${buildTypeDir} && cd build/${buildTypeDir}
                cmake ${buildTypeArg} ../..
                make -j\$(nproc)
                sudo make install
                sudo make package
                ldd -v /opt/rocm/lib/librocdecode.so
                """

    platform.runCommand(this, command)
}

def runTestCommand (platform, project) {

    String libLocation = ''

    if (platform.jenkinsLabel.contains('rhel')) {
        libLocation = ':/usr/local/lib'
    }
    else if (platform.jenkinsLabel.contains('sles')) {
        libLocation = ':/usr/local/lib'
    }

    def command = """#!/usr/bin/env bash
                set -x
                export HOME=/home/jenkins
                echo Make Test
                cd ${project.paths.project_build_prefix}/build/release
                LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/opt/rocm/lib${libLocation} make test ARGS="-VV --rerun-failed --output-on-failure"
                """

    platform.runCommand(this, command)
// Unit tests - TBD
}

def runPackageCommand(platform, project) {

    def packageHelper = platform.makePackage(platform.jenkinsLabel, "${project.paths.project_build_prefix}/build/release")

    String packageType = ''
    String packageInfo = ''
    String packageDetail = ''
    String osType = ''
    String packageRunTime = ''

    if (platform.jenkinsLabel.contains('centos') || platform.jenkinsLabel.contains('rhel') || platform.jenkinsLabel.contains('sles')) {
        packageType = 'rpm'
        packageInfo = 'rpm -qlp'
        packageDetail = 'rpm -qi'
        packageRunTime = 'rocdecode-*'

        if (platform.jenkinsLabel.contains('sles')) {
            osType = 'sles'
        }
        else if (platform.jenkinsLabel.contains('rhel8')) {
            osType = 'rhel8'
        }
        else if (platform.jenkinsLabel.contains('rhel9')) {
            osType = 'rhel9'
        }
    }
    else
    {
        packageType = 'deb'
        packageInfo = 'dpkg -c'
        packageDetail = 'dpkg -I'
        packageRunTime = 'rocdecode_*'

        if (platform.jenkinsLabel.contains('ubuntu20')) {
            osType = 'ubuntu20'
        }
        else if (platform.jenkinsLabel.contains('ubuntu22')) {
            osType = 'ubuntu22'
        }
    }

    def command = """#!/usr/bin/env bash
                set -x
                export HOME=/home/jenkins
                echo Make rocDecode Package
                cd ${project.paths.project_build_prefix}/build/release
                sudo make package
                mkdir -p package
                mv rocdecode-dev*.${packageType} package/${osType}-rocdecode-dev.${packageType}
                mv rocdecode-test*.${packageType} package/${osType}-rocdecode-test.${packageType}
                mv ${packageRunTime}.${packageType} package/${osType}-rocdecode.${packageType}
                mv Testing/Temporary/LastTest.log package/${osType}-LastTest.log
                mv Testing/Temporary/LastTestsFailed.log package/${osType}-LastTestsFailed.log
                ${packageDetail} package/${osType}-rocdecode-dev.${packageType}
                ${packageDetail} package/${osType}-rocdecode-test.${packageType}
                ${packageDetail} package/${osType}-rocdecode.${packageType}
                ${packageInfo} package/${osType}-rocdecode-dev.${packageType}
                ${packageInfo} package/${osType}-rocdecode-test.${packageType}
                ${packageInfo} package/${osType}-rocdecode.${packageType}
                """

    platform.runCommand(this, command)
    platform.archiveArtifacts(this, packageHelper[1])
}

return this