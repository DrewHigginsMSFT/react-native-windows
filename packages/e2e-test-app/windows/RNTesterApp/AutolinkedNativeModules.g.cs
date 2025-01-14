﻿// AutolinkedNativeModules.g.cs contents generated by "react-native autolink-windows"

using System.Collections.Generic;

// Namespaces from @react-native-picker/picker
using ReactNativePicker;

// Namespaces from @react-native-windows/automation-channel
using AutomationChannel;

namespace Microsoft.ReactNative.Managed
{
    internal static class AutolinkedNativeModules
    {
        internal static void RegisterAutolinkedNativeModulePackages(IList<IReactPackageProvider> packageProviders)
        { 
            // IReactPackageProviders from @react-native-picker/picker
            packageProviders.Add(new ReactNativePicker.ReactPackageProvider());
            // IReactPackageProviders from @react-native-windows/automation-channel
            packageProviders.Add(new AutomationChannel.ReactPackageProvider());
        }
    }
}
