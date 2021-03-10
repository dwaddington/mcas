# MCAS Web Pages based on Gatsby Theme Carbon

## Install Packages

RHEL 8.3

```bash
sudo dnf install npm
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.1/install.sh | bash
```

Re-open terminal

```bash
nvm install 12
nvm use 12
npm install
npm install -g gatsby-cli
```

Package files installed in ~/.npm directory.

## Deployed at https://ibm.github.io/mcas/

To build and run development:

```
gatsby develop
```

To deploy to github.io site:

```
npm run deploy
```

