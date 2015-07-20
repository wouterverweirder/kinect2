{
  "targets": [
    {
      "target_name": "kinect2",
      "sources": [ "src/kinect2.cc" ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "$(KINECTSDK20_DIR)\\inc"
      ],
      "libraries": [
        "-l$(KINECTSDK20_DIR)\\lib\\x64\\kinect20.lib"
      ]
    }
  ]
}