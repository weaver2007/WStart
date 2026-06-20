function Component() {
}

Component.prototype.createOperations = function() {
    component.createOperations();

    if (installer.value("os") === "win") {
        var target = installer.value("TargetDir") + "/HotKeyManager.exe";
        component.addOperation("CreateShortcut", target, "@StartMenuDir@/WStart.lnk",
                               "workingDirectory=@TargetDir@",
                               "iconPath=" + target,
                               "description=WStart");
        component.addOperation("CreateShortcut", target, "@DesktopDir@/WStart.lnk",
                               "workingDirectory=@TargetDir@",
                               "iconPath=" + target,
                               "description=WStart");
    }
}
