function Component() {
}

Component.prototype.createOperations = function() {
    component.createOperations();

    if (installer.value("os") === "win") {
        var target = installer.value("TargetDir") + "/HotKeyManager.exe";
        component.addOperation("CreateShortcut", target, "@StartMenuDir@/HStart.lnk",
                               "workingDirectory=@TargetDir@",
                               "iconPath=" + target,
                               "description=HStart");
        component.addOperation("CreateShortcut", target, "@DesktopDir@/HStart.lnk",
                               "workingDirectory=@TargetDir@",
                               "iconPath=" + target,
                               "description=HStart");
    }
}
