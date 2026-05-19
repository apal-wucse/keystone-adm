final: prev: {
  keystonePkgs = {
    bootrom = final.callPackage ./bootrom { };
    u-boot = final.callPackage ./u-boot { };
    keystone-sm = final.callPackage ./keystone-sm { };
    keystone-sdk = final.callPackage ./keystone-sdk { };
    runtime = final.callPackage ./runtime { };
  };

  meta-sifive = final.callPackage ./meta-sifive { };

  linuxPackages_6_12 = prev.linuxPackages_6_12.extend (
    lfinal: lprev: {
      keystone-driver = lfinal.callPackage ./driver { };
    }
  );
}
