import qbs
import "subdir/ParentInOtherDir.qbs" as ParentInOtherDir
import "subdir/AGroupInOtherDir.qbs" as AGroupInOtherDir
import "subdir/OtherGroupInOtherDir.qbs" as OtherGroupInOtherDir
import "subdir/YetAnotherGroupInOtherDir.qbs" as YetAnotherGroupInOtherDir
import "subdir/AndAnotherGroupInOtherDir.qbs" as AndAnotherGroupInOtherDir

Project {
    ParentInOtherDir {
        name: "p1"
        Depends { name: "gm" }

        Group {
            files: ["referenced-from-product.txt"]
        }
        GroupInSameDir { }
        AGroupInOtherDir { }
        OtherGroupInOtherDir { }
        YetAnotherGroupInOtherDir { }
        AndAnotherGroupInOtherDir { }
    }

    Product {
        name: "p2"
        AGroupInOtherDir { }
    }
}
