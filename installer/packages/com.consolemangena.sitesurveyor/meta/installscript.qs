function Component()
{
    // Default constructor
}

Component.prototype.createOperations = function()
{
    // Call default implementation to extract files
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut", 
                               "@TargetDir@/SiteSurveyor.exe", 
                               "@StartMenuDir@/SiteSurveyor.lnk",
                               "workingDirectory=@TargetDir@");
                               
        component.addOperation("CreateShortcut", 
                               "@TargetDir@/SiteSurveyor.exe", 
                               "@DesktopDir@/SiteSurveyor.lnk",
                               "workingDirectory=@TargetDir@");
    }

    if (systemInfo.productType === "x11") {
        component.addOperation("CreateDesktopEntry", 
                               "/usr/share/applications/SiteSurveyor.desktop",
                               "Type=Application\nName=SiteSurveyor\nExec=@TargetDir@/SiteSurveyor\nIcon=@TargetDir@/icon.png\nTerminal=false\nCategories=Education;Science;");
    }
}
