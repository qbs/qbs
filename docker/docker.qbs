import qbs

Project {
    Product {
        Depends { name: "docker"; required: false }

        name: "qbs-docker-stretch"
        type: ["docker.docker-image"]
        builtByDefault: false
        condition: docker.present

        docker.imageTag: "qbsbuild/qbsdev:stretch"

        files: [
            "stretch/Dockerfile",
            "stretch/qtifwsilent.qs",
        ]
    }

    Product {
        Depends { name: "docker"; required: false }

        name: "qbs-docker-windowsservercore"
        type: ["docker.docker-image"]
        builtByDefault: false
        condition: docker.present

        docker.imageTag: "qbsbuild/qbsdev:windowsservercore"

        files: [
            "windowsservercore/Dockerfile",
            "windowsservercore/qtifwsilent.qs",
        ]
    }
}
