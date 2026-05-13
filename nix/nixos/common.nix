{
  config,
  lib,
  pkgs,
  modulesPath,
  ...
}:
let
  keystone-driver = config.boot.kernelPackages.callPackage ../pkgs/driver { };
in
{
  imports = [ "${modulesPath}/profiles/base.nix" ];

  nix = {
    checkConfig = true;

    settings = {
      auto-optimise-store = true;
      experimental-features = [
        "nix-command"
        "flakes"
      ];
      trusted-users = [ "nixos" ];
    };
  };

  nixpkgs.hostPlatform = "riscv64-linux";

  boot.kernelPackages = pkgs.linuxPackages_6_12;

  boot.kernelPatches = [
    {
      name = "cma";
      patch = null;
      structuredExtraConfig = with lib.kernel; {
        CMA = yes;
        CMA_SIZE_SEL_MBYTES = yes;
        CMA_SIZE_MBYTES = freeform "1024";
        CMA_ALIGNMENT = freeform "9";
        CMA_DEBUG = yes;
        CMA_DEBUGFS = yes;
        CMA_SYSFS = yes;
        DMA_CMA = yes;
      };
    }
  ];

  boot.extraModulePackages = [ keystone-driver ];
  boot.kernelModules = [ "keystone-driver" ];

  services.openssh.enable = true;
  services.openssh.settings.PermitRootLogin = "yes";
  networking.firewall.allowedTCPPorts = [ 22 ];

  users.users.root.initialPassword = "nixos";
  users.users.nixos = {
    isNormalUser = true;
    extraGroups = [ "wheel" ];
    initialPassword = "nixos";
  };

  environment.systemPackages = with pkgs; [
    vim
    git
    tmux
    htop
    pciutils
    usbutils
    fastfetch
  ];

  system.stateVersion = "25.11";
}
