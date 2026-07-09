
[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp)用のZMKファームウェア

* サポート対象は「右が trackball、左が trackpad-mini」の構成のみです
* 非 dongle 構成では `_central` を右側、`_peripheral` を左側に書き込んでください
* dongle 構成では `right_peripheral` を右側、`left_peripheral` を左側、`dongle_central` を dongle に書き込んでください
* キーマップはkeymap-editorで編集できます

## 構成名

このリポジトリでは、artifact 名を完成構成ベースで付けています

* `nondongle_ball_right`: 非 dongle 構成。右が trackball、左が trackpad-mini
* `dongle_ball_right`: dongle 構成。右が trackball、左が trackpad-mini

## Artifact 一覧

`nondongle_ball_right`

* `torabo_tsuki_lp_nondongle_ball_right_right_central`
* `torabo_tsuki_lp_nondongle_ball_right_left_peripheral`

`dongle_ball_right`

* `torabo_tsuki_lp_dongle_ball_right_right_peripheral`
* `torabo_tsuki_lp_dongle_ball_right_left_peripheral`
* `torabo_tsuki_lp_dongle_ball_right_dongle_central`
