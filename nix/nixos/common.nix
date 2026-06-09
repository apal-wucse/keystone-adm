{
  inputs,
  config,
  lib,
  pkgs,
  modulesPath,
  keystoneOverlay,
  ...
}:
{
  imports = [ "${modulesPath}/profiles/base.nix" ];

  nix = {
    checkConfig = true;

    nixPath = lib.mkForce [
      "nixpkgs=/nix/var/nix/profiles/per-user/root/channels/nixpkgs"
    ];

    settings = {
      auto-optimise-store = true;
      experimental-features = [
        "nix-command"
        "flakes"
      ];
      trusted-users = [ "nixos" ];
    };
  };

  nixpkgs = {
    hostPlatform = "riscv64-linux";
    overlays = [ keystoneOverlay ];
  };

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

  boot.extraModulePackages = [ pkgs.linuxPackages_6_12.keystone-driver ];
  boot.kernelModules = [ "keystone-driver" ];

  boot.kernel.sysctl."kernel.perf_event_paranoid" = 0;

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
    perf
    gdb
    binutils
    strace
  ];

  environment.etc."nix/registry.json".text = builtins.toJSON {
    version = 2;
    flakes = [
      {
        from = {
          id = "nixpkgs";
          type = "indirect";
        };
        to = {
          type = "path";
          path = "${pkgs.path}";
        };
      }
    ];
  };

  system.activationScripts.linkNixpkgs = ''
    mkdir -p /nix/var/nix/profiles/per-user/root/channels
    ln -sfT ${inputs.nixpkgs} /nix/var/nix/profiles/per-user/root/channels/nixpkgs
  '';

  system.stateVersion = "25.11";
}
